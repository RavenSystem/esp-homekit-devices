#!/bin/sh

# Args: 1=FileName, 2=CN, 3=AltName
function build_test_cert_conf {
	echo "[ req ]" 										>  $1.conf
	echo "prompt = no"									>> $1.conf
	echo "default_bits        = 2048" 					>> $1.conf
	echo "distinguished_name  = req_distinguished_name" >> $1.conf
	echo "req_extensions      = req_ext" 				>> $1.conf
	echo "" 											>> $1.conf
	echo "[ req_distinguished_name ]" 					>> $1.conf
	echo "C = US" 										>> $1.conf
	echo "ST = Montana" 								>> $1.conf
	echo "L = Bozeman" 									>> $1.conf
	echo "OU = Engineering" 							>> $1.conf
	echo "CN = $2"  									>> $1.conf
	echo "emailAddress = info@wolfssl.com" 				>> $1.conf
	echo "" 											>> $1.conf
	echo "[ req_ext ]" 									>> $1.conf
	if [ -n "$3" ]; then
		if [[ "$3" != *"DER"* ]]; then
			echo "subjectAltName = @alt_names"			>> $1.conf
			echo "[alt_names]"							>> $1.conf
			echo "DNS.1 = $3"							>> $1.conf
		else
			echo "subjectAltName = $3" 					>> $1.conf
		fi
	fi
}

# Args: 1=FileName
function generate_test_cert {
	rm $1.der
	rm $1.pem

	echo "step 1 create configuration"
	build_test_cert_conf $1 $2 $3

	echo "step 2 create csr"
	openssl req -new -sha256 -out $1.csr -key ../server-key.pem -config $1.conf

	echo "step 3 check csr"
	openssl req -text -noout -in $1.csr

	echo "step 4 create cert"
	openssl x509 -req -days 1000 -sha256 -in $1.csr -signkey ../server-key.pem \
	             -out $1.pem -extensions req_ext -extfile $1.conf
	rm $1.conf
	rm $1.csr

	if [ -n "$4" ]; then
		echo "step 5 generate crl"
		mkdir ../crl/demoCA
		touch ../crl/demoCA/index.txt
	    echo "01" > ../crl/crlnumber
		openssl ca -config ../renewcerts/wolfssl.cnf -gencrl -crldays 1000 -out crl.revoked -keyfile ../server-key.pem -cert $1.pem
		rm ../crl/$1Crl.pem
		openssl crl -in crl.revoked -text > tmp.pem
		mv tmp.pem ../crl/$1Crl.pem
		rm crl.revoked
		rm -rf ../crl/demoCA
		rm ../crl/crlnumber*
	fi

	echo "step 6 add cert text information to pem"
	openssl x509 -inform pem -in $1.pem -text > tmp.pem
	mv tmp.pem $1.pem

	echo "step 7 make binary der version"
	openssl x509 -inform pem -in $1.pem -outform der -out $1.der
}


# Generate Good CN=localhost, Alt=None
generate_test_cert server-goodcn localhost "" 1

# Generate Good CN=www.nomatch.com, Alt=localhost
generate_test_cert server-goodalt www.nomatch.com localhost 1

# Generate Good CN=*localhost, Alt=None
generate_test_cert server-goodcnwild *localhost "" 1

# Generate Good CN=www.nomatch.com, Alt=*localhost
generate_test_cert server-goodaltwild www.nomatch.com *localhost 1

# Generate Bad CN=localhost\0h, Alt=None
# DG: Have not found a way to properly encode null in common name
generate_test_cert server-badcnnull DER:30:0d:82:0b:6c:6f:63:61:6c:68:6f:73:74:00:68

# Generate Bad Name CN=www.nomatch.com, Alt=None
generate_test_cert server-badcn www.nomatch.com

# Generate Bad Alt CN=www.nomatch.com, Alt=localhost\0h
generate_test_cert server-badaltnull www.nomatch.com DER:30:0d:82:0b:6c:6f:63:61:6c:68:6f:73:74:00:68

# Generate Bad Alt Name CN=www.nomatch.com, Alt=www.nomatch.com
generate_test_cert server-badaltname www.nomatch.com www.nomatch.com

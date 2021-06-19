// AWS IoT client endpoint
const char *client_endpoint = "<your-prefix>.iot.<aws-region>.amazonaws.com";

// AWS IoT device certificate (ECC)
const char *client_cert =
"-----BEGIN CERTIFICATE-----\r\n"
"------------------ <your client certificate> -------------------\r\n"
"-----END CERTIFICATE-----\r\n";

// AWS IoT device private key (ECC)
const char *client_key = 
"-----BEGIN EC PARAMETERS-----\r\n"
"BggqhkjOPQMBBw==\r\n"
"-----END EC PARAMETERS-----\r\n"
"-----BEGIN EC PRIVATE KEY-----\r\n"
"------------------ <your client private key> -------------------\r\n"
"-----END EC PRIVATE KEY-----\r\n";

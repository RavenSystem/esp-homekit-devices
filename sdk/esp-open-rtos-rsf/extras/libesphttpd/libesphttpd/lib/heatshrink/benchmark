#!/bin/sh

BENCHMARK_OUT=benchmark_out
HS=../heatshrink

cd ${BENCHMARK_OUT}

# Files in the Canterbury Corpus
# http://corpus.canterbury.ac.nz/resources/cantrbry.tar.gz
FILES='alice29.txt
asyoulik.txt
cp.html
fields.c
grammar.lsp
kennedy.xls
lcet10.txt
plrabn12.txt
ptt5
sum
xargs.1'

rm -f benchmark.output

# Run several combinations of -w W -l L,
# note compression ratios and check uncompressed output matches input
for W in 6 7 8 9 10 11 12; do
    for L in 5 6 7 8; do
        if [ $L -lt $W ]; then
            for f in ${FILES}
            do 
                IN_FILE="${f}"
                COMPRESSED_FILE="${f}.hsz.${W}_${L}"
                UNCOMPRESSED_FILE="${f}.orig.${W}_${L}"
                ${HS} -e -v -w ${W} -l ${L} ${IN_FILE} ${COMPRESSED_FILE} >> benchmark.output
                ${HS} -d -v -w ${W} -l ${L} ${COMPRESSED_FILE} ${UNCOMPRESSED_FILE} > /dev/null

                # compare file sizes
                if [ $(ls -l ${IN_FILE} | awk '{print($5)}') != $(ls -l ${UNCOMPRESSED_FILE} | awk '{print($5)}') ];
                then
                    printf "\n\n\nWARNING: size of %s does not match size of %s\n\n\n" \
			${IN_FILE} ${UNCOMPRESSED_FILE}
                else
                    printf "pass: -w %2d -l %2d  %s\n" ${W} ${L} "${f}"
                fi

                rm ${COMPRESSED_FILE} ${UNCOMPRESSED_FILE}
            done
        fi
    done
done

# Print totals and averages
awk '{ t += $2; c++ }; END { printf("====\nTotal compression: %.02f%% for %d documents (avg. %0.2f%%)\n", t, c, t / c) }' benchmark.output

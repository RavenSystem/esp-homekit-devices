#!/usr/bin/env python

import re
import sys


def gen_embedded(content):
    content = re.sub(r'{{.*?}}', '%s', re.sub(r'{%.*?%}', '', content))

    part_headers = list(re.finditer(r'<!-- part (\S+) -->', content))
    if not part_headers:
        print 'Error: no parts found'
        return

    def process_part(part):
        return "\n".join(
            "\"" + line.replace('"', '\\"') + "\""
            for line in part.split("\n")
            if line.strip()
        )

    result = ''

    for p1, p2 in zip(part_headers, part_headers[1:]):
        result += "const char *%s = \n%s;\n\n" % (
            p1.group(1).lower(),
            process_part(content[p1.end():p2.start()])
        )

    result += "const char *%s = \n%s;" % (
        part_headers[-1].group(1).lower(),
        process_part(content[part_headers[-1].end():])
    )

    return result


def main():
    file_path = sys.argv[1]
    with open(file_path) as f:
        print gen_embedded(f.read())


if __name__ == '__main__':
    main()

import RefParser from '@apidevtools/json-schema-ref-parser'
import fs from 'fs/promises'
;(async function main() {
  const schema = await RefParser.dereference(
    'https://raw.githubusercontent.com/hennessyevan/esp-homekit-schema/main/meplhaa.json'
  )
  await fs.writeFile('schema.json', JSON.stringify(schema, null, 2))
})()

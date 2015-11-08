import glob from 'glob'

glob('./upd_source/*.test.js', (error, files) => {
  if (error) {
    throw error
  }
  files.forEach(filePath => require(filePath))
})

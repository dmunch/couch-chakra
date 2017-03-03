module.exports = {
    entry:  __dirname + "/js/couch_chakra.js",
    output: {
        path:  __dirname + "/dist",
        filename: "couch_chakra.js",
        libraryTarget: "umd",
        library: "couch_chakra"
    }
}

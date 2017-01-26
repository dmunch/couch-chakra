Couch Chakra
------------

couch-chakra is a (sort of) drop in replacement for the CouchDB Query Server 
Runtime [couchjs](https://github.com/apache/couchdb-couch/tree/master/priv/couch_js) 
build on top of Microsoft's [ChakraCore](https://github.com/Microsoft/ChakraCore).

### Motivation

- Learning the inner workings of the CouchDB Query Server Subsystem
- Get my C skills back out of the closet
- Have fun working with fancy technology
- ChakraCore is used in Microsoft's [DocumentDB](https://azure.microsoft.com/en-us/blog/the-road-ahead-for-azure-documentdb-with-chakracore/)

### Status

- It currently passes all tests in [query_server_spec.rb](https://github.com/apache/couchdb/blob/master/test/view_server/query_server_spec.rb)
- Has proper sandboxing
- Deals with the anonymous function issue, aka [COUCHDB-1397](https://issues.apache.org/jira/browse/COUCHDB-1397)
- NO `for each`, so you'll have to patch [views.js](https://github.com/apache/couchdb/blob/master/share/server/views.js#L112) before running the query server tests
- NO cURL bindings
- NO `-T` command line flag for test suite specific functions 
- NO `-u` command line argument
- NO `--no-eval` command line argument 


### Discussion

It's not the first alternative suggested to the existing Query Server Runtime.
There's at least one based on NodeJS which started an interesting discussion at [COUCHDB-1894](https://issues.apache.org/jira/browse/COUCHDB-1894). 
A couple of important aspects for a Query Server Runtime can be extracted from it:

- Sandboxing
- The anonymous function issue
- Versioning of the runtime
- Ease of installation


### The anonymous function issue, aka [COUCHDB-1397](https://issues.apache.org/jira/browse/COUCHDB-1397)

In short, CouchDB makes use of a now disabled feature of the SpiderMonkey runtime 
which allowed anonymous functions to appear in global scope. This made for a really nice way of declaring
view functions in CouchDB, however it was always invalid behavior and no modern runtime is going to add this feature again.

Couch-Chakra has two propositions to work around this issue.

#### The legacy mode

Code written in the old CouchDB style `function(doc, view) {...}` is considered legacy. 
Running Couch-Chakra with the `-L` flag parses the provided script with a patched
version of [esprima](https://github.com/dmunch/esprima/commit/68cee92d15dd773029bb6ac7e31acc57a574ff05), rewrites
the resulting AST and generates a compliant version of the function with the help of [escodegen](https://github.com/estools/escodegen).
This is quite a huge machinery and I do hope that a modern JS runtime can make up for the performance hit, however 
I can't provide any figures. 

#### ES6 arrow functions

Fortunately ES6 introduced so called [arrow functions](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/Arrow_functions) 
which seem to be a perfect fit for CouchDB map functions. Replacing `function(doc, view) {...}` by 
`(doc, view) => {}` is a rather simple modification, simple to explain and transforms the not-so-valid JavaScript into
perfectly ES6 code which can be run without the legacy mode enabled.


## Build it

Couch-Chakra builds with current master of [ChakraCore](https://github.com/Microsoft/ChakraCore) (I used cb7817b) which I'm afraid you have to
build from sources. Once built, modify the first two lines of the [Makefile](Makefile) of this project
and run `make`. If you want to run the (two very simplistic) tests, run `make check` . Everythings
is very early, it builds on my machine™ which runs on MacOS X 10.11.6 with latest xcode.


## References

- [CouchDB 2.0 Query Server Interface Proposal](https://docs.google.com/document/d/1JtfvCpNB9pRQyLhS5KkkEdJ-ghSCv89xnw5HDMTCsp8/edit)
- [COUCHDB-1743 Make the view server & protocol faster](https://issues.apache.org/jira/browse/COUCHDB-1743)
- [COUCHDB-1894 Add experimental NodeJS query server](https://issues.apache.org/jira/browse/COUCHDB-1894)
- [COUCHDB-1643 Switch to V8](https://issues.apache.org/jira/browse/COUCHDB-1643)
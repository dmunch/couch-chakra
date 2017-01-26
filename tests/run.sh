#!/bin/bash
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

TESTS_DIR=$(dirname $0)
CHAKRA_BIN=$TESTS_DIR/../bin/couch-chakra
CHAI_JS=$TESTS_DIR/../obj/chai.js

for filename in $TESTS_DIR/*.js; do
  
  #extract command line parameters for couch_chakra
  header=$(head -1 $filename)
  if [[ ${header:0:2} == "//" ]] ; then 
    params=${header#"//"}
  fi
  
  pipeHeader=$(head -2 $filename | tail -1)
  if [[ ${pipeHeader:0:2} == "//" ]] ; then 
    pipe=${pipeHeader#"//"}
    eval $pipe | $CHAKRA_BIN -d $params $CHAI_JS "$filename"
  else
    $CHAKRA_BIN -d $params $CHAI_JS "$filename"
  fi

  rc=$?
  if [[ $rc != 0 ]]; then
    echo -e "$filename ${RED}failed${NC}." 
  else
    echo -e "$filename ${GREEN}passed${NC}." 
  fi 
done

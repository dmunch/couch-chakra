async function test() {
  while(true) {
    var arrayBuffer = await read_async();
    var uiBuffer = new Uint8Array(arrayBuffer);

    for(var i = 0; i < uiBuffer.length; i++) {
      await write_async(uiBuffer[i] + "-");
    }
    //await write_async(uiBuffer.toString());
  }
}

test();

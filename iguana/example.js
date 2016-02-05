// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function moduleDidLoad() {
  common.hideModule();
  }

function $(id) {
  return document.getElementById(id);
}

/*
 * file system functions
 * 
 */
window.requestFileSystem  = window.requestFileSystem || window.webkitRequestFileSystem;

function errorHandler(e,callback,name) {
  var msg = '';

  switch (e.code) {
    case FileError.QUOTA_EXCEEDED_ERR:
      msg = 'QUOTA_EXCEEDED_ERR';
      break;
    case FileError.NOT_FOUND_ERR:
      msg = 'NOT_FOUND_ERR';
      callback(name);
      break;
    case FileError.SECURITY_ERR:
      msg = 'SECURITY_ERR';
      break;
    case FileError.INVALID_MODIFICATION_ERR:
      msg = 'INVALID_MODIFICATION_ERR';
      break;
    case FileError.INVALID_STATE_ERR:
      msg = 'INVALID_STATE_ERR';
      break;
    default:
      msg = 'Unknown Error';
      break;
  };

  console.log('Error: ' + msg);
}
var fileSystem=null;
var files={};
function onInitFs(fs) {
  console.log('Opened file system: ' + fs.name);
  fileSystem = fs;
}

// Called by the common.js module.
function domContentLoaded(name, tc, config, width, height) {
  navigator.webkitPersistentStorage.requestQuota(10000000000, 
  function(bytes){
    window.requestFileSystem(PERSISTENT, bytes, onInitFs, errorHandler);
     common.updateStatus(
            'Allocated ' + bytes + ' bytes of persistent storage. Running the first time will take 17 seconds to load');
        common.attachDefaultListeners();
        common.createNaClModule(name, tc, config, width, height);
  }, function(e){
    console.log('Error', e);
});

}

var check_files=function(){
    
 var coins= coinManagement.getCoinSymbols();
               var files=["_hdrs.txt","_peers.txt"];
        for(var i=0;i<coins.length;i++){
         
            for(var j=0;j<files.length;j++){
                var name="confs/"+coins[i]+files[j];
                console.log("checking file "+name);
                files[name]=null;
             check_if_file_present(name);   
            }
        }       
           
};

var check_if_file_present=function(filename,callback){
 fileSystem.root.getFile(filename, {}, function(fileEntry) {
                 //console.log("entered file fu");
    // Get a File object representing the file,
    // then use FileReader to read its contents.
    fileEntry.file(function(file) {
       var reader = new FileReader();

       reader.onloadend = function(e) {
         //var txtArea = document.createElement('textarea');
         //console.log("Configuration file text: "+this.result.toString());
         console.log("File already present in HTML5 system:"+fileEntry.fullPath);
         //SPNAPI.conf_files[filename]=this.result.toString();
         };

       reader.readAsText(file);
    },  function(e){
        errorHandler(e,access_and_save_conf_file,filename);
   
});

  },  function(e){
        errorHandler(e,access_and_save_conf_file,filename);
   
});   
    
};

var access_and_save_conf_file=function(name){
    console.log("access file called for "+name);
    $.ajax({
    type: "GET",
    url: name,
    //async: false,
            success: function (data){
                save_contents(data,name);
            }
        });
        
};


var save_contents=function(contents,name){
    
    fileSystem.root.getFile(name, {create: true}, function(fileEntry) {

    // Create a FileWriter object for our FileEntry (log.txt).
    fileEntry.createWriter(function(fileWriter) {

      fileWriter.onwriteend = function(e) {
        console.log('Write completed.');
      };

      fileWriter.onerror = function(e) {
        console.log('Write failed: ' + e.toString());
      };
      
      // Create a new Blob and write it to log.txt.
      var blob = new Blob([contents], {type: 'text/plain'});

      fileWriter.write(blob);

    }, errorHandler);

  }, errorHandler);
};

// Called by the common.js module.
function attachListeners() {
  var radioEls = document.querySelectorAll('input[type="radio"]');
  for (var i = 0; i < radioEls.length; ++i) {
    radioEls[i].addEventListener('click', onRadioClicked);
  }

  // Wire up the 'click' event for each function's button.
  var functionEls = document.querySelectorAll('.function');
  for (var i = 0; i < functionEls.length; ++i) {
    var functionEl = functionEls[i];
    var id = functionEl.getAttribute('id');
    var buttonEl = functionEl.querySelector('button');

    // The function name matches the element id.
    var func = window[id];
    buttonEl.addEventListener('click', func);
  }
  //$('pipe_input_box').addEventListener('keypress', onPipeInput)
  //$('pipe_output').disabled = true;
  //$('pipe_name').addEventListener('change', function() { $('pipe_output').value = ''; })
}

// Called with keypress events on the pipe input box
function onPipeInput(e) {
  // Create an arraybuffer containing the 16-bit char code
  // from the keypress event.
  var buffer = new ArrayBuffer(1*2);
  var bufferView = new Uint16Array(buffer);
  bufferView[0] = e.charCode;

  // Pass the buffer in a dictionary over the NaCl module
  var pipeSelect = $('pipe_name');
  var pipeName = pipeSelect[pipeSelect.selectedIndex].value;
  var message = {
    pipe: pipeName,
    operation: 'write',
    payload: buffer,
  };
  nacl_module.postMessage(message);
  e.preventDefault();
  return false;
}

function onRadioClicked(e) {
  var divId = this.id.slice(5);  // skip "radio"
  var functionEls = document.querySelectorAll('.function');
  for (var i = 0; i < functionEls.length; ++i) {
    var visible = functionEls[i].id === divId;
    if (functionEls[i].id === divId)
      functionEls[i].removeAttribute('hidden');
    else
      functionEls[i].setAttribute('hidden', '');
  }
}

function addNameToSelectElements(cssClass, handle, name) {
  var text = '[' + handle + '] ' + name;
  var selectEls = document.querySelectorAll(cssClass);
  for (var i = 0; i < selectEls.length; ++i) {
    var optionEl = document.createElement('option');
    optionEl.setAttribute('value', handle);
    optionEl.appendChild(document.createTextNode(text));
    selectEls[i].appendChild(optionEl);
  }
}

function removeNameFromSelectElements(cssClass, handle) {
  var optionEls = document.querySelectorAll(cssClass + ' > option');
  for (var i = 0; i < optionEls.length; ++i) {
    var optionEl = optionEls[i];
    if (optionEl.value == handle) {
      var selectEl = optionEl.parentNode;
      selectEl.removeChild(optionEl);
    }
  }
}

var funcToCallback = {};

function postCall(func) {
  var callback = arguments[arguments.length - 1];
  funcToCallback[func] = callback;

  nacl_module.postMessage({
    cmd: func,
    args: Array.prototype.slice.call(arguments, 1, -1)
  });
}

function ArrayBufferToString(buf) { return String.fromCharCode.apply(null, new Uint16Array(buf)); }

// Called by the common.js module.
function handleMessage(message_event) {
  var data = message_event.data;
  if ((typeof(data) === 'string' || data instanceof String)) {
    common.logMessage(data);
  }
  else if (data instanceof Object)
  {
    var pipeName = data['pipe']
    if ( pipeName !== undefined )
    {
      // Message for JavaScript I/O pipe
      var operation = data['operation'];
      if (operation == 'write') {
        $('pipe_output').value += ArrayBufferToString(data['payload']);
      } else if (operation == 'ack') {
        common.logMessage(pipeName + ": ack:" + data['payload']);
      } else {
        common.logMessage('Got unexpected pipe operation: ' + operation);
      }
    }
    else
    {
      // Result from a function call.
      var params = data.args;
      var funcName = data.cmd;
      var callback = funcToCallback[funcName];
      if (!callback)
      {
        common.logMessage('Error: Bad message ' + funcName + ' received from NaCl module.');
        return;
      }
      delete funcToCallback[funcName];
      callback.apply(null, params);
    }
  } else {
    common.logMessage('Error: Unknow message `' + data + '` received from NaCl module.');
  }
}

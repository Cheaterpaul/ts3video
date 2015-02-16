(function () {
  "use strict";
  
  /////////////////////////////////////////////////////////////////////
  // AppMain
  /////////////////////////////////////////////////////////////////////

  function AppMain(address, port) {
    this.websocket = null;
    this.address = address;
    this.port = port;
  }
  
  AppMain.prototype.connect = function () {
    var that = this;
    if (this.websocket !== null) {
      return;
    }
    
    this.websocket = new WebSocket("ws://" + this.address + ":" + this.port + "/ts3video-websocket");
    
    this.websocket.onopen = function (ev) {
      that.websocket.send("/status");
    };
    
    this.websocket.onclose = function (ev) {
      that.websocket = null;
    };
    
    this.websocket.onerror = function (ev) {
      that.websocket = null;
      jQuery("#AppMain").html("Server connection lost... Reconnecting...");
      setTimeout(function () { that.connect(); }, 3000);
    };
    
    this.websocket.onmessage = function (ev) {
      var data = JSON.parse(ev.data);
      brite.display("MainView", "#AppMain", data, { emptyParent: true })
        .done(function () {
          setTimeout(function () {
            if (that.websocket !== null) {
              that.websocket.send("/status");
            }
          }, 1500); // TODO (The server should send update instead of requesting it!)
        })
        .fail(function () {
        });
    };
  };
  
  /////////////////////////////////////////////////////////////////////

  function round(f) {
    return parseFloat(Math.round(f * 100) / 100).toFixed(2);
  }

  $.views.helpers({
    bytesAsReadableSize: function (b) {
      if (b > 1073741824) {
        return round(b / 1073741824) + " GB";
      } else if (b > 1048576) {
        return round(b / 1048576) + " MB";
      } else if (b > 1024) {
        return round(b / 1024) + " KB";
      }
      return (b) + " Bytes";
    }
  });
  
  /////////////////////////////////////////////////////////////////////
  
  brite.viewDefaultConfig.loadTmpl = true;
  brite.viewDefaultConfig.loadCss = false;
  
  var app = new AppMain(window.location.host, 80);
  app.connect();

}());

/*
Code to test the webserver. This depends on:
- The cat images being available, for concurrent espfs testing
- the test.cgi script available, for generic data mangling tests


This test does a max of 4 requests in parallel. The nonos SDK supports a max of
5 connections; the default libesphttpd setting is 4 sockets at a time. Unfortunately,
the nonos sdk just closes all sockets opened after the available sockets are opened,
instead of queueing them until a socket frees up.
*/


function log(line) {
	$("#log").insertAdjacentHTML('beforeend', line+'<br />');
}


//Load an image multiple times in parallel
function testParLdImg(url, ct, doneFn) {
	var im=[];
	var state={"loaded":0, "count":ct, "doneFn": doneFn, "error":false};
	for (var x=0; x<ct; x++) {
		im[x]=new Image();
		im[x].onload=function(no) {
			log("File "+no+" loaded successfully.");
			this.loaded++;
			if (this.loaded==this.count) this.doneFn(!this.error);
		}.bind(state, x);
		im[x].onerror=function(no) {
			log("Error loading image "+no+"!");
			this.loaded++;
			this.error++;
			if (this.loaded==this.count) this.doneFn(!this.error);
		}.bind(state, x);
		im[x].src=url+"?"+Math.floor(Math.random()*100000).toString();
	}
}

function testDownloadCgi(len, doneFn) {
	var xhr=j();
	var state={"len":len, "doneFn":doneFn, "ts": Date.now()};
	xhr.open("GET", "test.cgi?len="+len+"&nocache="+Math.floor(Math.random()*100000).toString());
	xhr.onreadystatechange=function() {
		if (xhr.readyState==4 && xhr.status>=200 && xhr.status<300) {
			if (xhr.response.length==this.len) {
				log("Downloaded "+this.len+" bytes successfully.");
				this.doneFn(true);
			} else {
				log("Downloaded "+xhr.response.length+" bytes successfully, but needed "+this.len+"!");
				this.doneFn(false);
			}
		} else if (xhr.readyState==4) {
			log("Failed! Error "+xhr.status);
			this.doneFn(false);
		}
	}.bind(state);
	//If the webbrowser enables it, show progress.
	if (typeof xhr.onprogress != 'undefined') {
		xhr.onprogress=function(e) {
			if (Date.now()>this.ts+2000) {
				log("..."+Math.floor(e.loaded*100/this.len).toString()+"%");
				this.ts=Date.now();
			}
		}.bind(state);
	}
	xhr.send();
}


function testUploadCgi(len, doneFn) {
	var xhr=j();
	var state={"len":len, "doneFn":doneFn, "ts": Date.now()};
	var data="";
	for (var x=0; x<len; x++) data+="X";
	xhr.open("POST", "test.cgi");
	xhr.onreadystatechange=function() {
		if (xhr.readyState==4 && xhr.status>=200 && xhr.status<300) {
			var ulen=parseInt(xhr.responseText);
			if (ulen==this.len) {
				log("Uploaded "+this.len+" bytes successfully.");
				this.doneFn(true);
			} else {
				log("Webserver received "+ulen+" bytes successfully, but sent "+this.len+"!");
				this.doneFn(false);
			}
		} else if (xhr.readyState==4) {
			log("Failed! Error "+xhr.status);
			this.doneFn(false);
		}
	}.bind(state);
	//If the webbrowser enables it, show progress.
	if (typeof xhr.upload.onprogress != 'undefined') {
		xhr.upload.onprogress=function(e) {
			if (Date.now()>this.ts+2000) {
				log("..."+Math.floor(e.loaded*100/e.total).toString()+"%");
				this.ts=Date.now();
			}
		}.bind(state);
	}
	//Upload the file
	xhr.send(data);
}

function hammerNext(state, xhr) {
	if (state.done==state.count) {
		state.doneFn(!state.error);
	}
	if (state.started==state.count) return;
	xhr.open("GET", "test.cgi?len="+state.len+"&nocache="+Math.floor(Math.random()*100000).toString());
	xhr.onreadystatechange=function(xhr) {
		if (xhr.readyState==4 && xhr.status>=200 && xhr.status<300) {
			if (xhr.response.length==this.len) {
				state.done++;
				hammerNext(this, xhr);
			} else {
				log("Downloaded "+xhr.response.length+" bytes successfully, but needed "+this.len+"!");
				state.done++;
				hammerNext(this, xhr);
			}
		} else if (xhr.readyState==4) {
			log("Failed! Error "+xhr.status);
			state.done++;
			hammerNext(this, xhr);
		}
	}.bind(state, xhr);
	//If the webbrowser enables it, show progress.
	if (typeof xhr.onprogress != 'undefined') {
		xhr.onprogress=function(e) {
			if (Date.now()>this.ts+2000) {
				log("..."+state.done+"/"+state.count);
				this.ts=Date.now();
			}
		}.bind(state);
	}
	state.started++;
	xhr.send();
}

function testHammer(count, par, len, doneFn) {
	var state={"count":count, "started":0, "done":0, "par":par, "len":len, "doneFn":doneFn, "ts": Date.now(), "error":false};
	var xhr=[];
	for (var i=0; i<par; i++) {
		xhr[i]=j();
		hammerNext(state, xhr[i]);
	}
}



var tstState=0;
var successCnt=0;

function nextTest(lastOk) {
	if (tstState!=0) {
		if (lastOk) {
			log("<b>Success!</b>"); 
			successCnt++;
		} else {
			log("<b>Test failed!</b>");
		}
	}
	tstState++;
	if (tstState==1) {
		log("Testing parallel load of espfs files...");
		testParLdImg("../cats/kitten-loves-toy.jpg", 3, nextTest);
	} else if (tstState==2) {
		log("Testing GET request of 32K...");
		testDownloadCgi(32*1024, nextTest);
	} else if (tstState==3) {
		log("Testing GET request of 128K...");
		testDownloadCgi(128*1024, nextTest);
	} else if (tstState==4) {
		log("Testing GET request of 512K...");
		testDownloadCgi(512*1024, nextTest);
	} else if (tstState==5) {
		log("Testing POST request of 512 bytes...");
		testUploadCgi(512, nextTest);
	} else if (tstState==6) {
		log("Testing POST request of 16K bytes...");
		testUploadCgi(16*1024, nextTest);
	} else if (tstState==7) {
		log("Testing POST request of 512K bytes...");
		testUploadCgi(512*1024, nextTest);
	} else if (tstState==8) {
		log("Hammering webserver with 500 requests of size 512...");
		testHammer(500, 3, 512, nextTest);
	} else if (tstState==9) {
		log("Hammering webserver with 500 requests of size 2048...");
		testHammer(500, 3, 2048, nextTest);
	} else {
		log("Tests done! "+successCnt+" out of "+(tstState-1)+" tests were successful.");
	}
}



window.onload=function(e) {
	log("Starting tests.");
	nextTest(false);
}




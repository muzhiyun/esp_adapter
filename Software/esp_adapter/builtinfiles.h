/**
 * @file builtinfiles.h
 * @brief This file is part of the WebServer example for the ESP8266WebServer.
 *  
 * This file contains long, multiline text variables for  all builtin resources.
 */

// used for $upload.htm
static const char FileManagerHtml[] PROGMEM =
R"==(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Files Manager</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f4f4f4;
        }

        h1 {
            text-align: center;
            color: #333;
        }

        a {
            text-decoration: none;
            color: #007bff;
            margin-bottom: 10px;
        }

        #list {
            margin-top: 20px;
        }

        #list div {
            background-color: #fff;
            border: 1px solid #ddd;
            padding: 10px;
            margin-bottom: 10px;
            border-radius: 4px;
        }

        #list a {
            text-decoration: none;
            color: #007bff;
        }

        #list a:hover {
            text-decoration: underline;
        }

        .deleteFile {
            color: #ff6347;
            cursor: pointer;
            margin-left: 10px;
        }

        .deleteFile:hover {
            text-decoration: underline;
        }

        span {
            color: #555;
        }

        #zone {
            width: 100%;
            height: 200px;
            padding: 20px;
            background-color: #f0f0f0;
            border: 2px dashed #ccc;
            text-align: center;
            line-height: 200px;
            color: #666;
            font-size: 1.2em;
            cursor: pointer;
            margin-top: 20px;
        }

        #zone:hover {
            background-color: #e0e0e0;
        }

        .file-upload-wrapper {
            position: relative;
            margin-top: 20px;
        }

        .file-upload-input {
            width: 100%;
            margin-bottom: 10px;
        }

        .file-upload-button {
            display: block;
            width: 100%;
            padding: 10px;
            text-align: center;
            background: #007bff;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
        }

        .file-upload-button:hover {
            background: #0056b3;
        }
    </style>
</head>

<body>
    <h1>Files Manager</h1>
    <a href="/">Home</a>
    <hr>

    <p>These files are available on the server to be opened or deleted:</p>
    <div id="list">
    </div>

    <div id='zone'>Drop files here for upload...</div>

    <div class="file-upload-wrapper">
        <input type="file" id="file-upload" class="file-upload-input" multiple>
        <button type="button" class="file-upload-button" id="file-upload-btn">Upload Files</button>
    </div>

    <script>
        // allow drag&drop of file objects 
        function dragHelper(e) {
            e.stopPropagation();
            e.preventDefault();
        }

        // allow drag&drop of file objects 
        function dropped(e) {
            dragHelper(e);
            var fls = e.dataTransfer.files;
            uploadFiles(fls);
        }

        // Load and display all files after page loading has finished
        window.addEventListener("load", function () {
            fetch('/list')
                .then(function (result) { return result.json(); })
                .then(function (e) {
                    var listObj = document.querySelector('#list');
                    e.forEach(function (f) {
                        var entry = document.createElement("div");
                        var nameObj = document.createElement("a");
                        nameObj.href = '/' + f.name;
                        nameObj.innerText = f.name;
                        entry.appendChild(nameObj)

                        entry.appendChild(document.createTextNode(' (' + f.size + ' bytes) '));

                        var timeObj = document.createElement("span");
                        timeObj.innerText = (new Date(f.time*1000)).toLocaleString();
                        entry.appendChild(timeObj)

                        var delObj = document.createElement("span");
                        delObj.className = 'deleteFile';
                        delObj.innerText = 'delete';
                        delObj.setAttribute('data-filename', f.name);
                        entry.appendChild(delObj)

                        listObj.appendChild(entry)
                    });

                })
                .catch(function (err) {
                    window.alert(err);
                });
        });

        window.addEventListener("click", function (evt) {
            var t = evt.target;
            if (t.className === 'deleteFile') {
                var fname = t.getAttribute('data-filename');
                if (window.confirm("Delete " + fname + " ?")) {
                    var delfile = encodeURIComponent(fname); 
                    fetch('/files?delfile=' + delfile, { method: 'DELETE' })
                        .then(function(response) {
                            if (!response.ok) {
                                throw new Error('Network response was not ok');
                            }
                            document.location.reload(false);
                        })
                        .catch(function(error) {
                            console.error('Error:', error);
                        });
                }
            };
        });


        function uploadFiles(files) {
            var uploadBtn = document.getElementById('file-upload-btn');
            uploadBtn.innerText = 'Uploading...Please wait';
            uploadBtn.disabled = true;

            var formData = new FormData();
            for (var i = 0; i < files.length; i++) {
                formData.append('file', files[i], files[i].name);
            }
            fetch('/files', { method: 'POST', body: formData })
                .then(function (response) {
                    if (!response.ok) {
                        throw new Error('Network response was not ok');
                    }
                    return response.text();
                })
                .then(function () {
                    window.alert('Upload successful.');
                    document.location.reload(false); 
                })
                .catch(function (error) {
                    console.error('Error:', error);
                    window.alert('Upload failed.');
                })
                .finally(function () {
                    uploadBtn.innerText = 'Upload Files';
                    uploadBtn.disabled = false;
            });
        }


        var z = document.getElementById('zone');
        z.addEventListener('dragenter', dragHelper, false);
        z.addEventListener('dragover', dragHelper, false);
        z.addEventListener('drop', dropped, false);

        document.addEventListener('DOMContentLoaded', function() {
            var uploadBtn = document.getElementById('file-upload-btn');
            var fileInput = document.getElementById('file-upload');
            uploadBtn.addEventListener('click', function() {
                var files = fileInput.files;
                uploadFiles(files);
            });
        });
    </script>
</body>

</html>


)==";

static const char WiFiHtml[] PROGMEM =
R"==(
<!DOCTYPE html>
<html>
<head>
    <title>WiFi Setting</title>
    <meta charset="UTF-8">
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
        }
        .container {
            margin: 0 auto;
            width: 50%;
            padding: 20px;
            border: 1px solid #ccc;
            border-radius: 5px;
            background-color: #f9f9f9;
        }
        input[type="text"], input[type="password"], input[type="submit"] {
            width: 100%;
            padding: 10px;
            margin: 5px 0;
            display: inline-block;
            border: 1px solid #ccc;
            box-sizing: border-box;
            border-radius: 5px;
        }
        input[type="submit"] {
            background-color: #0056b3;
            color: white;
            border: none;
            cursor: pointer;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>WiFi Setting</h2>
        <form id="wifiForm" onsubmit="submitForm(event)">
            <input type="text" id="wifiSSID" placeholder="WiFi ssid" required><br>
            <input type="password" id="wifiPassword" placeholder="WiFi Password" required><br>
            <input type="submit" value="Submit">
        </form>
    </div>

    <script>
        function submitForm(event) {
            event.preventDefault();
            var ssid = document.getElementById("wifiSSID").value;
            var password = document.getElementById("wifiPassword").value;
            
            var url = "/wifi";
            var params = "ssid=" + encodeURIComponent(ssid) + "&password=" + encodeURIComponent(password);
            var xhr = new XMLHttpRequest();
            xhr.open("POST", url, true);
            xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
            xhr.onload = function() {
                if (xhr.status == 200) {
                    alert("配置已写入,请手动重启");
                } else {
                    alert("配置写入失败");
                }
            };
            xhr.send(params);
        }
    </script>
</body>
</html>

)==";


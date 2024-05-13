/**
 * @file builtinfiles.h
 * @brief This file is part of the WebServer example for the ESP8266WebServer.
 *  
 * This file contains long, multiline text variables for  all builtin resources.
 */

// used for $upload.htm
static const char uploadContent[] PROGMEM =
R"==(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Upload</title>
    <link rel="stylesheet" href="bootstrap.min.css">
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 400px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f9f9f9;
        }

        h1 {
            text-align: center;
            color: #333;
        }

        a {
            text-decoration: none;
            color: #007bff;
            display: block;
            margin-bottom: 10px;
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
    </style>
</head>

<body>
    <h1>Upload</h1>
    <a href="/">Home</a>
    <hr>
    <div id='zone'>Drop files here...</div>

    <script src="jquery-3.6.0.min.js"></script>
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
            var formData = new FormData();
            for (var i = 0; i < fls.length; i++) {
                formData.append('file', fls[i], '/' + fls[i].name);
            }
            fetch('/', { method: 'POST', body: formData }).then(function () {
                window.alert('done.');
            });
        }

        var z = document.getElementById('zone');
        z.addEventListener('dragenter', dragHelper, false);
        z.addEventListener('dragover', dragHelper, false);
        z.addEventListener('drop', dropped, false);
    </script>
</body>

</html>

)==";

// used for $upload.htm
static const char notFoundContent[] PROGMEM = R"==(
<html>
<head>
  <title>Ressource not found</title>
</head>
<body>
  <p>The ressource was not found.</p>
  <p><a href="/">Start again</a></p>
</body>
)==";

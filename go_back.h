const char GO_BACK[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en" xml:lang="en">

<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <title>WifiRGB Controller Interface</title>
    <script type="text/javascript" src="iro.min.js"></script>
    <script type="text/javascript" src="https://code.jquery.com/jquery-3.3.1.min.js"></script>
    <style>
      body {
        color:#fff;
        background:#272727;
        text-align: center;
        align-items: center;
        font-size:300%;
        font-family:verdana;
        justify-content: center;
      }

      #back {
        background-color: #C4C4C4;
        border: 1px;
        color: black;
        font-size: 20px;
        width: 35%;
        font-weight: bold;
        font-family: "verdana";
        border-radius: 20px;
        padding-left: 10px;
        padding-right: 10px;
        padding-top: 10px;
        padding-bottom: 10px;
        cursor: pointer;
        margin-bottom: 6%;
      }
    </style>
</head>
<body>
  <br>
  <input type="button" id="back" value="Go Back" onclick="history.back(-1)" />
</body>
</html>
)=====";
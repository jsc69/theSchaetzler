const char webpage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<body style="background-color: #f9e79f ">
<center>
<div>
<h1>theSch&auml;tzler</h1>
</div>
<div><h2>
  value(mm): <span id="value">0</span><br><br>
</h2>
</div>
<script>
setInterval(function() 
{
  getData();
}, 1000); 
function getData() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("value").innerHTML =
      this.responseText;
    }
  };
  xhttp.open("GET", "read", true);
  xhttp.send();
}
</script>
</center>
</body>
</html>
)=====";
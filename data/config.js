
document.addEventListener('DOMContentLoaded', function()
{
  document.addEventListener('ws_state', ws_state_change);
  document.addEventListener('ws_message', process_ws_message);
  document.getElementById('scan').addEventListener('click', scan_wifi);
  document.getElementById('update').addEventListener('click', config_update);
  document.getElementById('ssids').addEventListener('change', ssid_select);
  openWS();
});

function ws_state_change(evt)
{
  if (evt.detail.obj['open'])
  {
    let msg = '{"func": "get_wifi", "path": "' + document.location.pathname + '"}';
    console.log(msg);
    sendToWS(msg);
  }
}

function process_ws_message(evt)
{
  try
  {
    let msg = JSON.parse(evt.detail.message);
    console.log(msg);
    let func = msg.func;
    if (func == "wifi_resp")
    {
      document.getElementById('hostname').value = msg['host'];
      document.getElementById('ssid').value = msg['ssid'];
      document.getElementById('ip').innerHTML = msg['ip'];
      document.getElementById('timezone').value = msg['timezone'];
    }
    else if (func == "wifi-ssids")
    {
      let html = '<option>-- Choose WiFi --</option>';
      for(let ii = 0; ii < msg.ssids.length; ii++)
      {
        html += '<option>' + msg.ssids[ii].name + '</option>';
      }
      document.getElementById('ssids').innerHTML = html;
    }
  }
  catch(e)
  {
    console.log(e);
  }
  ping_ = true;
}

function scan_wifi()
{
  document.getElementById('ssids').innerHTML = '<option>-- Scanning --</option>';
  let msg = '{"func": "scan_wifi", "path": "' + document.location.pathname + '"}';
  console.log(msg);
  sendToWS(msg);
}

function config_update()
{
  document.getElementById('ip').innerHTML = '';
  let cmd = '{"func": "config_update"';
  let inps = document.querySelectorAll('input');
  for (let inp of inps)
  {
    cmd += ' ,"' + inp.name + '": "' + encodeURI(inp.value) + '"';
  }
  cmd += ', "path": "' + document.location.pathname + '"}"';
  console.log(cmd);
  sendToWS(cmd);
}

function ssid_select()
{
  let sel = document.getElementById('ssids');
  let idx = sel.selectedIndex;
  if (idx > 0)
  {
    let ssid = document.getElementById('ssid');
    ssid.value = sel.options[idx].value;
  }
}

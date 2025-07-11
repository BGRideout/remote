//  Functions for test IR page

document.addEventListener('DOMContentLoaded', function()
{
    document.addEventListener('ws_message', process_ws_message);
    openWS();
});

function load_ir()
{
    let ntc = document.getElementById("ntc");
    ntc.innerHTML = "Click button on remote";
    let msg = '{"func": "ir_get", "ir_get": "0", "path": "' + document.location.pathname + '"}'
    console.log(msg);
    sendToWS(msg);
}

function send_ir()
{
    let ntc = document.getElementById("ntc");
    let typ = document.getElementById("typ").value;
    let add = parseInt(document.getElementById("add").value);
    let val = parseInt(document.getElementById("val").value);
    if (Number.isNaN(add) || Number.isNaN(val))
    {
        ntc.innerHTML = "Addr and Cmd must be numbers";
    }
    else
    {
        let msg = '{"func":"test_send", "type":"' + typ + '", "address":"' + add + '", "value":"' + val +
                    '", "path": "' + document.location.pathname + '"}'
        console.log(msg);
        ntc.innerHTML = "Sending ...";
        sendToWS(msg);
    }
}

function process_ws_message(evt)
{
    try
    {
        let ntc = document.getElementById("ntc");
        let msg = JSON.parse(evt.detail.message);
        console.log(msg);
        let func = msg.func;
        if (func == 'ir_resp' && msg.type != "")
        {
            let ele = document.getElementById("typ");
            ele.value = msg.type;

            ele = document.getElementById("add");
            ele.value = msg.address;

            ele = document.getElementById("val");
            ele.value = msg.value;

            ntc.innerHTML = "&nbsp";
        }
        else if (func == "send_resp")
        {
            ntc.innerHTML = "Sent";
            setTimeout(()=>{document.getElementById("ntc").innerHTML = "&nbsp";}, 750);
        }
        else
        {
            ntc.innerHTML = "Did not read a known code";
        }
    }
    catch(e)
    {
        console.log(e);
    }
}

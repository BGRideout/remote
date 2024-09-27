//  Functions for setup button page

document.addEventListener('DOMContentLoaded', function()
{
    document.addEventListener('ws_message', process_ws_message);
    openWS();
});

function load_ir(row)
{
    let ntc = document.getElementById("irget");
    ntc.innerHTML = "Click button on remote";

    let msg = '{"ir_get": ' + row + ', "path": "' + document.location.pathname + '"}'
    console.log(msg);
    sendToWS(msg);

}

function process_ws_message(evt)
{
    try
    {
        let msg = JSON.parse(evt.detail.message);
        console.log(msg);
        if (Object.hasOwn(msg, 'ir_resp'))
        {
            console.log("IR message");
            let ntc = document.getElementById("irget");
            ntc.innerHTML = "";
        }
    }
    catch(e)
    {
        console.log(e);
    }
}

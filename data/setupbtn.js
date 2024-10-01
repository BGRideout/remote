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

    let msg = '{"func": "ir_get", "ir_get": ' + row + ', "path": "' + document.location.pathname + '"}'
    console.log(msg);
    sendToWS(msg);

}

function process_ws_message(evt)
{
    try
    {
        let msg = JSON.parse(evt.detail.message);
        console.log(msg);
        if (Object.hasOwn(msg, 'ir_resp') && msg.type != "")
        {
            let ntc = document.getElementById("irget");
            ntc.innerHTML = "&nbsp;";

            let steps = document.getElementById("steps");
            let rows = steps.getElementsByTagName("tr");
            let row = rows.item(msg.ir_resp);
            let inputs = row.getElementsByTagName("input");
            for (inp of inputs)
            {
                if (inp.name == "typ")
                {
                    inp.value = msg.type;
                }
                else if (inp.name == "add")
                {
                    inp.value = msg.address;
                }
                else if (inp.name == "val")
                {
                    inp.value = msg.value;
                }
                else if (inp.name == "dly")
                {
                    if (inp.value == "")
                    {
                        inp.value = msg.delay;
                    }
                }
            }
            document.getElementById("actForm").submit();
        }
        else
        {
            let ntc = document.getElementById("irget");
            ntc.innerHTML = "Did not read a known code";
        }
    }
    catch(e)
    {
        console.log(e);
    }
}

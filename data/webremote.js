
// Event handling
var event_time = undefined;     // Last event time
var click_timer = undefined     // Click timer
var ping_ = true;
 
document.addEventListener("DOMContentLoaded", function()
{
    openWS();
    let frm = document.getElementById("btnForm")
    frm.addEventListener("submit", function(event)
    {
        if (typeof ws == "object")
        {
            event.preventDefault();
        }
    });
    
    let btns = document.querySelectorAll(".buttons button");
    for (btn of btns)
    {
        addButtonEvents(btn)
    }
    
    document.addEventListener('ws_message', process_ws_message);
    showLED("off");
});

function addButtonEvents(btn)
{
    btn.addEventListener("pointerdown", processPointerEvent);
    btn.addEventListener("pointerup", processPointerEvent);
    btn.addEventListener("pointerleave", processPointerEvent);
}

function process_ws_message(evt)
{
    let obj = JSON.parse(evt.detail.message);
    console.log(obj);
    if (obj.action !== undefined)
    {
        showLED("off");
        if (obj.action == "press" || obj.action == "busy")
        {
            showLED("on");
        }
        else if (obj.action == "no-repeat")
        {
            event_time = undefined;
        }
    }
    else
    {
        showLED("off");
    }
    
    if (obj.redirect !== undefined && obj.redirect != "" && obj.action != "busy")
    {
        let loc = document.location;
        document.location = loc.origin + obj.redirect;
    }
}

function processPointerEvent(event)
{
    let ele = event.srcElement
    if (typeof ele == "object" && ele.name === "btnVal")
    {
        if (event.type == "pointerdown")
        {
            if (event.buttons == 1)
            {
                event_time = event.timeStamp;
                click_timer = setTimeout(start_hold, 250, ele.value);
            }
            else
            {
                event_time = undefined
            }
        }
        else if (event.type == "pointerup")
        {
            if (event_time !== undefined)
            {
                let dur = event.timeStamp - event_time;
                event_time = undefined;
                let action = "release";
                if (click_timer !== undefined)
                {
                    clearTimeout(click_timer);
                    click_timer = undefined;
                    action = "click";
                    showLED("on");
                }
                
                sendToWS('{"btnVal": "' + ele.value +
                          '", "action": "' + action +
                          '", "duration": "' + dur +
                          '", "path": "' + document.location.pathname + '" }');
            }
        }
        else
        {
            if (event_time !== undefined)
            {
                let dur = event.timeStamp - event_time;
                sendToWS('{"btnVal": "' + ele.value +
                          '", "action": "' + 'cancel' +
                          '", "duration": "' + dur +
                          '", "path": "' + document.location.pathname + '" }');
            }
            event_time = undefined
            if (click_timer !== undefined)
            {
                clearTimeout(click_timer);
                click_timer = undefined;
            }
        }
    }
}

function start_hold(ix)
{
    click_timer = undefined
    sendToWS('{"btnVal": "' + ix + '", ' +
              '"action": "press", ' +
              '"path": "' + document.location.pathname + '" }');
}

function showLED(state)
{
    led = document.getElementById("led")
    if (state == "on")
    {
        led.innerHTML = "<svg viewbox='0 0 25 25' width='25' height='25' xmlns='http://www.w3.org/2000/svg'>" +
                        "<circle cx='12' cy='12' r='12' fill='red' stroke='white' stroke-width='3' /></svg>"
    }
    else
    {
        led.innerHTML = "<svg viewbox='0 0 25 25' width='25' height='25' xmlns='http://www.w3.org/2000/svg'>" +
                        "<circle cx='12' cy='12' r='12' fill='#660000' stroke='whitesmoke' stroke-width='3' /></svg>"
    }
}


function load_ir(row)
{
    let ntc = document.getElementById("irget");
    ntc.innerHTML = "Click button on remote";
    url = document.location.href;
    if (! url.endsWith("/"))
    {
        url += "/"
    }
    document.location = url + row;
}

function load_ir_msg()
{
    let ntc = document.getElementById("msg");
    ntc.innerHTML = "Click button on remote";
}

function insert_row(row)
{
    url = document.location.href;
    if (! url.endsWith("/"))
    {
        url += "/"
    }
    document.location = url + "?add=" + row;
}

function apsel()
{
    let sel = document.getElementById("sel");
    let idx = sel.selectedIndex;
    if (idx > 0)
    {
        let ssid = document.getElementById("ssid");
        ssid.value = sel.options[idx].innerHTML;
    }
}

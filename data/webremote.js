
// Event handling
var event_time = undefined;     // Last event time
var click_timer = undefined     // Click timer
var ping_ = true;
 
document.addEventListener("DOMContentLoaded", function()
{
    openWS();
    
    let btns = document.querySelectorAll(".buttons button");
    for (let btn of btns)
    {
        addButtonEvents(btn)
    }
    
    document.addEventListener('ws_message', process_ws_message);
    document.addEventListener('ws_state', ()=>{showLED("off");});
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
    let func = obj.func;
    if (func == "btn_resp")
    {
        if (obj.action !== undefined)
        {
            showLED("off");
            if (obj.action == "press")
            {
                showLED("on");
            }
            else if (obj.action == "busy")
            {
                showLED("busy");
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
            if (obj.redirect.startsWith("http://") || obj.redirect.startsWith("https://"))
            {
                window.open(obj.redirect, "redirect");
            }
            else
            {
                let loc = document.location;
                document.location = loc.origin + obj.redirect;
            }
        }
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
                
                sendToWS('{"func": "btnVal", "btnVal": "' + ele.value +
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
                sendToWS('{"func": "btnVal", "btnVal": "' + ele.value +
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
    sendToWS('{"func": "btnVal", "btnVal": "' + ix + '", ' +
              '"action": "press", ' +
              '"path": "' + document.location.pathname + '" }');
}

function showLED(state)
{
    led = document.getElementById("led")
    if (isWSOpen())
    {
        if (state == "on")
        {
            led.innerHTML = "<svg viewbox='0 0 25 25' width='25' height='25' xmlns='http://www.w3.org/2000/svg'>" +
                            "<circle cx='12' cy='12' r='12' fill='red' stroke='white' stroke-width='3' /></svg>"
        }
        else if (state == "busy")
        {
            led.innerHTML = "<svg viewbox='0 0 25 25' width='25' height='25' xmlns='http://www.w3.org/2000/svg'>" +
                            "<circle cx='12' cy='12' r='12' fill='red' stroke='orange' stroke-width='3' /></svg>"
        }
        else
        {
            led.innerHTML = "<svg viewbox='0 0 25 25' width='25' height='25' xmlns='http://www.w3.org/2000/svg'>" +
                            "<circle cx='12' cy='12' r='12' fill='#660000' stroke='white' stroke-width='3' /></svg>"
        }
    }
    else
    {
        led.innerHTML = "<svg viewbox='0 0 25 25' width='25' height='25' xmlns='http://www.w3.org/2000/svg'>" +
        "<circle cx='12' cy='12' r='12' fill='black' stroke='black' stroke-width='3' /></svg>"
    }
}

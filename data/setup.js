//  Functions for setup page

function btnAction(key)
{
    url = document.location.href;
    console.log(url);
    url.replace(".html", "");
    if (! url.endsWith("/"))
    {
        url += "/"
    }
    console.log(url)
    document.location = url + key;
}

function btnDone()
{
     let db = document.getElementById("done")
     let sts = db.getAttribute("class")
     if (sts == "saved" || confirm("Changes not saved! Close anyway?"))
     {
         btnAction(0)
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

//  Functions for setup page

function btnAction(key)
{
    let url = new URL(document.location.href);
    document.location = url.pathname.replaceAll(".html", "") + "/" + key;
}

function btnDone()
{
    let url = new URL(document.location.href);
    let db = document.getElementById("done")
    let sts = db.getAttribute("class")
    if (sts == "saved" || confirm("Changes not saved! Close anyway?"))
    {
       document.location = url.pathname.replaceAll(".html", "") + "?done=true"
    }
}

function load_ir(row)
{
    let ntc = document.getElementById("irget");
    ntc.innerHTML = "Click button on remote";
    // url = document.location.href;
    // if (! url.endsWith("/"))
    // {
    //     url += "/"
    // }
    // document.location = url + row;
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

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

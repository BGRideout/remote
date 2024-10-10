//  Navigation menu on double click of header

'use strict';

document.addEventListener("DOMContentLoaded", () =>
{
    let h1 = document.getElementsByTagName("h1");
    if (h1 && h1.length > 0)
    {
        h1 = h1.item(0);
        h1.addEventListener("dblclick", showNavigator);
        window.addEventListener("mousedown", hideNavigator);

        let html = '<div id="navigator" class="ctxMenu">';
        if (document.getElementById("led"))
        {
            html += '<button type="button" dest="setup">Setup</button><br>';
        }
        else
        {
            html += '<button type="button" dest="/">Home</button><br>';
        }
        html += '<button type="button" dest="/menu">Menu Edit</button><br>' +
            '<button type="button" dest="/config">Configuration</button><br>' +
            '<button type="button" dest="/backup">Backup</button><br>' +
            '<button type="button" dest="/test">IR Test</button><br>' +
            '<button type="button" dest="/log">Log</button><br>' +
            '</div>';
        h1.insertAdjacentHTML('beforebegin', html);

        let nav = document.getElementById("navigator");
        let eles = nav.getElementsByTagName("button");
        for (let ii = 0; ii < eles.length; ii++)
        {
            let ele = eles[ii];
            if (ele.getAttribute("dest") !== null);
            {
                ele.addEventListener("mousedown", navigateTo);
            }
        }
    
    }
});

function showNavigator()
{
    let nav = document.getElementById("navigator");
    let x = visualViewport.width / 2 - 75;
    nav.style.left = x + 'px';
    nav.style.visibility = "visible";
}

function hideNavigator()
{
    let nav = document.getElementById("navigator");
    nav.style.visibility = "hidden";
}

function navigateTo(evt)
{
    evt.preventDefault();
    hideNavigator();
    let dest = evt.target.getAttribute("dest");
    console.log(dest);
    if (dest.startsWith("/"))
    {
        document.location = dest;
    }
    else
    {
        let path = document.location.pathname;
        path = path.replace(".html", "");
        path = path.replace("/index", "/");
        if (!path.endsWith("/")) path += "/";
        console.log(path, dest);
        document.location = path + dest;
    }
}
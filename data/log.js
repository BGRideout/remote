//  Functions for the log page

'use strict';

document.addEventListener("DOMContentLoaded", function()
{
    let log = document.getElementById("log");
    log.scrollTo(0,log.scrollHeight);
});

function submitForm()
{
    let frm = document.getElementById("btnForm");
    frm.submit();
}

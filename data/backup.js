 
document.addEventListener("DOMContentLoaded", function()
{
    let forms = document.getElementsByTagName("form");
    for (frm of forms)
    {
        frm.addEventListener("submit", clear_message);
    }
});

                          
function clear_message()
{
    let msg = document.getElementById("filemsg");
    msg.innerHTML = ""
}
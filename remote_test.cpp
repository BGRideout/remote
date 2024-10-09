//                 ***** Remote class "test" methods  *****

#include "remote.h"
#include "irdevice.h"
#include "command.h"
#include "txt.h"
#include "web_files.h"
#include <stdio.h>

bool Remote::test_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("test.html", data, datalen))
    {
        TXT html(data, datalen, 2048);

        std::string type_opts;
        std::vector<std::string> types;
        IR_Device::protocols(types);
        for (auto it = types.cbegin(); it != types.cend(); ++it)
        {
            type_opts += "<option value=\"" + *it + "\">" + *it + "</option>";
        }
        html.substitute("<?ir_types?>", type_opts);

        ret = send_http(web, client, html, close);
    }
    return ret;
}

bool Remote::test_send(WEB *web, ClientHandle client, const JSONMap &msgmap)
{
    Command *cmd = new Command(web, client, msgmap, nullptr);
    queue_command(cmd);
    return true;
}

bool Remote::test_ir_get(WEB *web, ClientHandle client, const JSONMap &msgmap)
{
    bool ret = true;
    if (isDebug(1)) log_->print("test_ir_get, path = %s\n", msgmap.strValue("path"));

    Command *cmd = new Command(web, client, msgmap, nullptr);
    queue_command(cmd);
    return ret;
}

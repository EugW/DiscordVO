#define _CRT_SECURE_NO_WARNINGS
#include <dpp/dpp.h>
#include <iomanip>
#include <sstream>
#include <timeapi.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "token.h"
constexpr auto BUFSZ = 11520 * 10;

uint16_t* buf;
uint16_t* bufptr;
uint16_t* out;
bool rdy = false;
LARGE_INTEGER freq{};
LARGE_INTEGER last{};
FILE* fl = fopen("test.pcm", "wb");
bool cl = false;
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    for (uint16_t* x = (uint16_t*)pInput; x < (uint16_t*)pInput + frameCount; x++) {
        memcpy(bufptr, x, 2);
        bufptr++;
        memcpy(bufptr, x, 2);
        bufptr++;
    }
    if (bufptr - buf == BUFSZ) {
        memcpy(out, buf, BUFSZ * 2);
        rdy = true;
        bufptr = buf;
    }
    //std::cout << bufptr - buf << std::endl;
}
int main() {
    auto x = sizeof(uint16_t);
    QueryPerformanceFrequency(&freq);
    timeBeginPeriod(1);
    buf = (uint16_t*)malloc(BUFSZ * x);
    out = (uint16_t*)malloc(BUFSZ * x);
    bufptr = buf;

    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        printf("Failed to initialize context.\n");
        return -2;
    }
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    if (ma_context_get_devices(&context, NULL, NULL, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        printf("Failed to get capture devices.\n");
        return -2;
    }
    for (ma_uint32 iDevice = 0; iDevice < captureCount; iDevice += 1) {
        printf("%d - %s\n", iDevice, pCaptureInfos[iDevice].name);
    }
    std::cout << "Selected: " << pCaptureInfos[1].name << std::endl;
    ma_device_config deviceConfig;
    ma_device device;
    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.pDeviceID = &pCaptureInfos[1].id;
    deviceConfig.capture.format = ma_format_s16;
    deviceConfig.capture.channels = 1;
    deviceConfig.sampleRate = 48000;
    deviceConfig.dataCallback = data_callback;
    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to initialize capture device.\n");
        return -2;
    }
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to start device.\n");
        return -3;
    }

    /* Setup the bot */
    dpp::cluster bot(token, dpp::i_default_intents | dpp::i_message_content); // Privileged intent required to receive message content

    bot.on_log(dpp::utility::cout_logger());

    /* Use the on_message_create event to look for commands */
    bot.on_message_create([&bot](const dpp::message_create_t& event) {

        std::stringstream ss(event.msg.content);
        std::string command;

        ss >> command;

        /* Tell the bot to join the discord voice channel the user is on. Syntax: .join */
        if (command == ".join") {
            dpp::guild* g = dpp::find_guild(event.msg.guild_id);
            if (!g->connect_member_voice(event.msg.author.id)) {
                bot.message_create(dpp::message(event.msg.channel_id, "You don't seem to be on a voice channel! :("));
            }
        }

        /* Tell the bot to play the sound file 'Robot.pcm'. Syntax: .robot */
        if (command == ".robot") {
            dpp::voiceconn* v = event.from->get_voice(event.msg.guild_id);
            if (v && v->voiceclient && v->voiceclient->is_ready()) {
                v->voiceclient->set_send_audio_type(dpp::discord_voice_client::send_audio_type_t::satype_live_audio);
                while (true) {
                    while (!rdy) {
                    }
                    v->voiceclient->send_audio_raw((uint16_t*)out, BUFSZ * 2);
                    rdy = false;
                }
            }
        }
        });

    /* Start bot */
    bot.start(dpp::st_wait);
    ma_device_uninit(&device);
	return 0;
}

#include "MessageBox.h"
#include "StringUtils.h"

#include "SDL.h"

void MessageBox::ShowErrorMessage(const string& message, const string& traceback)
{
#if defined(FO_WEB) || defined(FO_ANDROID) || defined(FO_IOS)
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FOnline Error", message.c_str(), nullptr);

#else
    string verb_message = message;
#ifdef FO_WINDOWS
    string most_recent = "most recent call first";
#else
    string most_recent = "most recent call last";
#endif
    if (!traceback.empty())
        verb_message += _str("\n\nTraceback ({}):\n{}", most_recent, traceback);

    SDL_MessageBoxButtonData copy_button;
    SDL_zero(copy_button);
    copy_button.buttonid = 0;
    copy_button.text = "Copy";

    SDL_MessageBoxButtonData close_button;
    SDL_zero(close_button);
    close_button.buttonid = 1;
    close_button.text = "Close";

// Workaround for strange button focus behaviour
#ifdef FO_WINDOWS
    copy_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    copy_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
#else
    close_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    close_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
#endif

    const SDL_MessageBoxButtonData buttons[] = {close_button, copy_button};
    SDL_MessageBoxData data;
    SDL_zero(data);
    data.flags = SDL_MESSAGEBOX_ERROR;
    data.title = "FOnline Error";
    data.message = verb_message.c_str();
    data.numbuttons = 2;
    data.buttons = buttons;

    int buttonid;
    while (!SDL_ShowMessageBox(&data, &buttonid) && buttonid == 0)
        SDL_SetClipboardText(verb_message.c_str());
#endif
}

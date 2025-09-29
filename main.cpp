#include "chip8.h"
#include <SDL2/SDL.h>
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <wx/dir.h>
#include <vector>
#include <memory>

// -------------------------
// IDs for speed menu items
// -------------------------
enum
{
    ID_SPEED_FASTEST = wxID_HIGHEST + 1,
    ID_SPEED_FAST,
    ID_SPEED_NORMAL,
    ID_SPEED_SLOW
};

enum
{
    ID_SCREEN_CLASSIC = wxID_HIGHEST + 10,
    ID_SCREEN_GREEN
};

// Forward declare our GLCanvas
class Chip8Canvas;

// -------------------------
// Main application class
// -------------------------
class Chip8App : public wxApp
{
public:
    virtual bool OnInit() override;
};

// -------------------------
// OpenGL canvas
// -------------------------
class Chip8Canvas : public wxGLCanvas
{
public:
    Chip8Canvas(wxWindow *parent, Chip8 &chip8Ref)
        : wxGLCanvas(parent, wxID_ANY, nullptr),
          chip8(chip8Ref)
    {
        context = new wxGLContext(this);

        // Default speed: Normal
        emulationDelay = 2;

        // Timer for updates
        timer.SetOwner(this);
        timer.Start(1000 / 60); // 60 Hz
        Bind(wxEVT_TIMER, &Chip8Canvas::OnTimer, this);
        Bind(wxEVT_PAINT, &Chip8Canvas::OnPaint, this);
        Bind(wxEVT_KEY_DOWN, &Chip8Canvas::OnKeyDown, this);
        Bind(wxEVT_KEY_UP, &Chip8Canvas::OnKeyUp, this);
        Bind(wxEVT_SIZE, &Chip8Canvas::OnSize, this); // handle resizing

        SetFocus(); // Receive keyboard events

        // ---- SDL audio setup ----
        if (SDL_Init(SDL_INIT_AUDIO) == 0)
        {
            SDL_zero(audioSpec);
            audioSpec.freq = 44100;
            audioSpec.format = AUDIO_F32SYS;
            audioSpec.channels = 1;
            audioSpec.samples = 1024;
            audioSpec.callback = nullptr;

            audioDevice = SDL_OpenAudioDevice(nullptr, 0, &audioSpec, nullptr, 0);
            if (audioDevice != 0)
            {
                SDL_PauseAudioDevice(audioDevice, 0);
                audioInitialized = true;
            }
        }
    }

    ~Chip8Canvas()
    {
        if (audioInitialized)
        {
            SDL_CloseAudioDevice(audioDevice);
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
        delete context;
    }

    enum class ScreenFilter
    {
        Classic, // White pixels on black
        Green    // Green pixels on dark green
    };

    ScreenFilter filter = ScreenFilter::Classic;

    void OnSize(wxSizeEvent &)
    {
        int w, h;
        GetClientSize(&w, &h);

        // calculate scale to fit 64x32 while keeping aspect ratio
        int scaleX = w / 64;
        int scaleY = h / 32;
        int scale = std::max(1, std::min(scaleX, scaleY));

        // calculate actual viewport size
        int viewW = 64 * scale;
        int viewH = 32 * scale;

        // center the viewport
        int offsetX = (w - viewW) / 2;
        int offsetY = (h - viewH) / 2;

        SetCurrent(*context);
        glViewport(offsetX, offsetY, viewW, viewH);

        // Update projection matrix
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 64, 32, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void OnPaint(wxPaintEvent &)
    {
        wxPaintDC dc(this);
        SetCurrent(*context);
        Render();
        SwapBuffers();
    }

    void OnTimer(wxTimerEvent &)
    {
        if (paused)
            return; // do nothing if paused

        int cyclesToRun = 1; // default
        switch (emuSpeed)
        {
        case Speed::Slow:
            cyclesToRun = 2;
            break;
        case Speed::Normal:
            cyclesToRun = 5;
            break;
        case Speed::Fast:
            cyclesToRun = 10;
            break;
        case Speed::Fastest:
            cyclesToRun = 20;
            break;
        }

        for (int i = 0; i < cyclesToRun; ++i)
            chip8.emulateCycle();

        chip8.decrementTimers();

        // Beep handling (unchanged)
        if (chip8.beepFlag && audioInitialized)
        {
            const int durationMs = 50;
            const int sampleCount = (audioSpec.freq * durationMs) / 1000;
            std::vector<float> buffer(sampleCount);
            for (int i = 0; i < sampleCount; ++i)
                buffer[i] = (i % (audioSpec.freq / 440) < (audioSpec.freq / 880)) ? 0.25f : -0.25f;

            SDL_QueueAudio(audioDevice, buffer.data(), buffer.size() * sizeof(float));
        }

        Refresh();
    }

    void OnKeyDown(wxKeyEvent &event) { MapKey(event, true); }
    void OnKeyUp(wxKeyEvent &event) { MapKey(event, false); }

    void SetSpeed(int delayMs)
    {
        emulationDelay = delayMs;
    }

    enum class Speed
    {
        Slow,
        Normal,
        Fast,
        Fastest
    };

    Speed emuSpeed = Speed::Normal; // default speed
    bool paused = false;

    wxString currentROMPath;

private:
    void Render()
    {
        int w, h;
        GetSize(&w, &h);
        glViewport(0, 0, w, h);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 64, 32, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        switch (filter)
        {
        case ScreenFilter::Classic:
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // black
            break;
        case ScreenFilter::Green:
            glClearColor(155.0f / 255.0f, 188.0f / 255.0f, 15.0f / 255.0f, 1.0f); // #9bbc0f
            break;
        }
        glClear(GL_COLOR_BUFFER_BIT);

        glBegin(GL_QUADS);
        for (int y = 0; y < 32; ++y)
        {
            for (int x = 0; x < 64; ++x)
            {
                if (chip8.gfx[y * 64 + x])
                {
                    switch (filter)
                    {
                    case ScreenFilter::Classic:
                        glColor3f(1.0f, 1.0f, 1.0f); // white
                        break;
                    case ScreenFilter::Green:
                        glColor3f(15.0f / 255.0f, 56.0f / 255.0f, 15.0f / 255.0f); // #0f380f
                        break;
                    }
                    glVertex2f(x, y);
                    glVertex2f(x + 1, y);
                    glVertex2f(x + 1, y + 1);
                    glVertex2f(x, y + 1);
                }
            }
        }
        glEnd();
    }

    void MapKey(wxKeyEvent &event, bool pressed)
    {
        int key = -1;
        switch (event.GetKeyCode())
        {
        case '1':
            key = 0x1;
            break;
        case '2':
            key = 0x2;
            break;
        case '3':
            key = 0x3;
            break;
        case '4':
            key = 0xC;
            break;
        case 'Q':
            key = 0x4;
            break;
        case 'W':
            key = 0x5;
            break;
        case 'E':
            key = 0x6;
            break;
        case 'R':
            key = 0xD;
            break;
        case 'A':
            key = 0x7;
            break;
        case 'S':
            key = 0x8;
            break;
        case 'D':
            key = 0x9;
            break;
        case 'F':
            key = 0xE;
            break;
        case 'Z':
            key = 0xA;
            break;
        case 'X':
            key = 0x0;
            break;
        case 'C':
            key = 0xB;
            break;
        case 'V':
            key = 0xF;
            break;
        }
        if (key != -1)
            chip8.keys[key] = pressed;
    }

    Chip8 &chip8;
    wxGLContext *context;
    wxTimer timer;

    // Audio
    SDL_AudioDeviceID audioDevice = 0;
    SDL_AudioSpec audioSpec{};
    bool audioInitialized = false;

    // Emulation speed
    int emulationDelay;
};

class PixelButton : public wxButton
{
public:
    PixelButton(wxWindow *parent, int id, const wxString &label)
        : wxButton(parent, id, label, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT | wxBORDER_NONE)
    {
        wxFont font(14, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        SetFont(font);
        SetBackgroundColour(wxColour(30, 30, 30));
        SetForegroundColour(wxColour(200, 200, 200));
        SetMinSize(wxSize(40, 40));
        SetMaxSize(wxSize(60, 60));

        pressed = false;

        Bind(wxEVT_LEFT_DOWN, &PixelButton::OnMouseDown, this);
        Bind(wxEVT_LEFT_UP, &PixelButton::OnMouseUp, this);
        Bind(wxEVT_LEAVE_WINDOW, &PixelButton::OnMouseLeave, this);
    }

protected:
    bool pressed;

    void OnMouseDown(wxMouseEvent &event)
    {
        pressed = true;
        Refresh();
        event.Skip(); // allow normal click processing
    }

    void OnMouseUp(wxMouseEvent &event)
    {
        pressed = false;
        Refresh();
        event.Skip();
    }

    void OnMouseLeave(wxMouseEvent &event)
    {
        pressed = false;
        Refresh();
        event.Skip();
    }

    void OnPaint(wxPaintEvent &WXUNUSED(event))
    {
        wxPaintDC dc(this);
        wxSize sz = GetClientSize();

        // Draw rounded rectangle
        int radius = 5; // pixelated rounded corners
        wxColour bg = GetBackgroundColour();
        wxColour border = wxColour(200, 200, 200);
        wxColour topShade = bg.ChangeLightness(150);   // highlight
        wxColour bottomShade = bg.ChangeLightness(50); // shadow

        // Fill background
        dc.SetBrush(wxBrush(bg));
        dc.SetPen(wxPen(border));
        dc.DrawRoundedRectangle(0, 0, sz.GetWidth(), sz.GetHeight(), radius);

        // Draw press shading
        if (pressed)
        {
            // darker inset
            dc.SetBrush(wxBrush(bottomShade));
            dc.DrawRoundedRectangle(2, 2, sz.GetWidth() - 4, sz.GetHeight() - 4, radius);
        }
        else
        {
            // lighter top-left to simulate 3D effect
            dc.SetPen(topShade);
            dc.DrawLine(0, 0, sz.GetWidth(), 0);  // top
            dc.DrawLine(0, 0, 0, sz.GetHeight()); // left

            dc.SetPen(bottomShade);
            dc.DrawLine(0, sz.GetHeight() - 1, sz.GetWidth(), sz.GetHeight() - 1); // bottom
            dc.DrawLine(sz.GetWidth() - 1, 0, sz.GetWidth() - 1, sz.GetHeight());  // right
        }

        // Draw text centered
        dc.SetTextForeground(GetForegroundColour());
        dc.SetFont(GetFont());
        wxSize textSize = dc.GetTextExtent(GetLabel());
        int tx = (sz.GetWidth() - textSize.GetWidth()) / 2;
        int ty = (sz.GetHeight() - textSize.GetHeight()) / 2;
        dc.DrawText(GetLabel(), tx, ty);
    }

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(PixelButton, wxButton)
    EVT_PAINT(PixelButton::OnPaint)
        wxEND_EVENT_TABLE()

    // -------------------------
    // Main frame (with canvas)
    // -------------------------
    class Chip8FrameWithCanvas : public wxFrame
{
public:
    Chip8FrameWithCanvas(Chip8 *chip8Ptr, const wxString &romFile)
        : wxFrame(nullptr, wxID_ANY, "CHIP-8 Emulator", wxDefaultPosition, wxSize(640, 480)),
          chip8(chip8Ptr)
    {
        SetIcon(wxICON(IDI_APP_ICON));

        // ---- Menu Bar ----
        wxMenu *fileMenu = new wxMenu;
        fileMenu->Append(wxID_OPEN, "Open Game\tCtrl+O");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_EXIT);

        wxMenu *emulationMenu = new wxMenu;
        emulationMenu->Append(wxID_STOP, "Pause\tCtrl+P");
        emulationMenu->Append(wxID_REFRESH, "Reset\tCtrl+R");

        wxMenu *speedMenu = new wxMenu;
        speedMenu->AppendRadioItem(ID_SPEED_FASTEST, "Fastest");
        speedMenu->AppendRadioItem(ID_SPEED_FAST, "Fast");
        speedMenu->AppendRadioItem(ID_SPEED_NORMAL, "Normal");
        speedMenu->AppendRadioItem(ID_SPEED_SLOW, "Slow");
        wxMenuItem *normalItem = speedMenu->FindItem(ID_SPEED_NORMAL);
        if (normalItem)
        {
            normalItem->Check(true);
        }
        emulationMenu->AppendSubMenu(speedMenu, "Speed");

        wxMenu *screenMenu = new wxMenu;
        screenMenu->AppendRadioItem(ID_SCREEN_CLASSIC, "Classic");
        screenMenu->AppendRadioItem(ID_SCREEN_GREEN, "Green");
        wxMenuItem *classicItem = screenMenu->FindItem(ID_SCREEN_CLASSIC);
        if (classicItem)
        {
            classicItem->Check(true);
        }

        wxMenuBar *menuBar = new wxMenuBar;
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(emulationMenu, "&Emulation");
        menuBar->Append(screenMenu, "&Screen");
        SetMenuBar(menuBar);

        // ---- Bind events ----
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnOpenGame, this, wxID_OPEN);
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnQuit, this, wxID_EXIT);
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnPause, this, wxID_STOP);
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnReset, this, wxID_REFRESH);
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnSpeedChange, this, ID_SPEED_FASTEST, ID_SPEED_SLOW);
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnScreenFilterChange, this, ID_SCREEN_CLASSIC, ID_SCREEN_GREEN);

        // ---- Layout ----
        wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

        // Canvas (bigger proportion)
        canvas = new Chip8Canvas(this, *chip8);
        canvas->emuSpeed = Chip8Canvas::Speed::Normal;
        mainSizer->Add(canvas, 3, wxEXPAND | wxALL, 5); // proportion=3 for more space

        // Keypad
        wxGridSizer *keypadSizer = new wxGridSizer(4, 4, 5, 5);
        const char *labels[16] = {"1", "2", "3", "C", "4", "5", "6", "D", "7", "8", "9", "E", "A", "0", "B", "F"};
        const int keyMap[16] = {
            0x1, 0x2, 0x3, 0xC, // 1 2 3 C
            0x4, 0x5, 0x6, 0xD, // 4 5 6 D
            0x7, 0x8, 0x9, 0xE, // 7 8 9 E
            0xA, 0x0, 0xB, 0xF  // A 0 B F
        };

        for (int i = 0; i < 16; ++i)
        {
            PixelButton *btn = new PixelButton(this, 1000 + i, labels[i]);
            keypadSizer->Add(btn, 1, wxEXPAND);

            btn->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { /* empty: preserves button clicking animation */ });

            btn->Bind(wxEVT_LEFT_DOWN, [=](wxMouseEvent &event)
                      {
                          chip8->keys[keyMap[i]] = true;
                          canvas->SetFocus();
                          event.Skip(); // allow button to process the click normally
                      });

            btn->Bind(wxEVT_LEFT_UP, [=](wxMouseEvent &event)
                      {
                          chip8->keys[keyMap[i]] = false;
                          canvas->SetFocus();
                          event.Skip(); // allow button to process the click normally
                      });
        }

        mainSizer->Add(keypadSizer, 1, wxEXPAND | wxALL, 10);
        SetSizer(mainSizer);
        Layout();
        canvas->SetFocus();

        // ---- Status Bar ----
        CreateStatusBar();
        LoadROM(romFile);
    }

private:
    void OnOpenGame(wxCommandEvent &)
    {
        wxFileDialog dlg(this, "Open ROM", "", "",
                         "CHIP-8 ROMs (*.ch8;*.rom)|*.ch8;*.rom|All files (*.*)|*.*",
                         wxFD_OPEN);
        if (dlg.ShowModal() == wxID_OK)
            LoadROM(dlg.GetPath());
    }

    void OnQuit(wxCommandEvent &) { Close(); }

    void LoadROM(const wxString &path)
    {
        chip8->reset();
        if (!chip8->loadROM(std::string(path.mb_str())))
        {
            wxMessageBox("Failed to load ROM", "Error", wxOK | wxICON_ERROR);
            SetStatusText("Failed to load ROM");
        }
        else
        {
            SetStatusText("Loaded ROM: " + path);
            canvas->currentROMPath = path; // store path for reload
        }
    }

    void OnPause(wxCommandEvent &)
    {
        canvas->paused = !canvas->paused;
        wxMenuItem *pauseItem = GetMenuBar()->FindItem(wxID_STOP);
        if (pauseItem)
            pauseItem->SetItemLabel(canvas->paused ? "Resume\tCtrl+P" : "Pause\tCtrl+P");

        SetStatusText(canvas->paused ? "Paused" : "Running");
    }

    void OnReset(wxCommandEvent &)
    {
        if (!canvas->currentROMPath.IsEmpty())
        {
            if (!chip8->loadROM(std::string(canvas->currentROMPath.mb_str())))
            {
                wxMessageBox("Failed to reload ROM", "Error", wxOK | wxICON_ERROR);
                SetStatusText("Failed to reload ROM");
            }
            else
            {
                SetStatusText("ROM reloaded");
            }
        }
        else
        {
            wxMessageBox("No ROM loaded to reset", "Error", wxOK | wxICON_ERROR);
        }
    }

    void OnSpeedChange(wxCommandEvent &event)
    {
        int id = event.GetId();
        switch (id)
        {
        case ID_SPEED_SLOW:
            canvas->emuSpeed = Chip8Canvas::Speed::Slow;
            break;
        case ID_SPEED_NORMAL:
            canvas->emuSpeed = Chip8Canvas::Speed::Normal;
            break;
        case ID_SPEED_FAST:
            canvas->emuSpeed = Chip8Canvas::Speed::Fast;
            break;
        case ID_SPEED_FASTEST:
            canvas->emuSpeed = Chip8Canvas::Speed::Fastest;
            break;
        }

        SetStatusText("Speed changed");
    }

    void OnScreenFilterChange(wxCommandEvent &event)
    {
        int id = event.GetId();
        switch (id)
        {
        case ID_SCREEN_CLASSIC:
            canvas->filter = Chip8Canvas::ScreenFilter::Classic;
            break;
        case ID_SCREEN_GREEN:
            canvas->filter = Chip8Canvas::ScreenFilter::Green;
            break;
        }
        canvas->Refresh(); // force redraw
    }

    Chip8 *chip8;
    Chip8Canvas *canvas;
};

// -------------------------
// Launcher frame
// -------------------------
class Chip8Frame : public wxFrame
{
public:
    Chip8Frame()
        : wxFrame(nullptr, wxID_ANY, "CHIP-8 Emulator", wxDefaultPosition, wxSize(640, 480))
    {
        SetIcon(wxICON(IDI_APP_ICON));

        chip8 = std::make_unique<Chip8>();

        wxMenu *fileMenu = new wxMenu;
        fileMenu->Append(wxID_FILE, "Open Game\tCtrl+O");
        fileMenu->Append(wxID_OPEN, "Open Folder...");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_EXIT);

        wxMenuBar *menuBar = new wxMenuBar;
        menuBar->Append(fileMenu, "&File");
        SetMenuBar(menuBar);

        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        romList = new wxListBox(this, wxID_ANY);
        playBtn = new wxButton(this, wxID_ANY, "Play Selected Game");
        playBtn->Disable();

        wxStaticText *hintText = new wxStaticText(this, wxID_ANY, "Open a folder to begin");
        sizer->Add(hintText, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 10);
        sizer->Add(romList, 1, wxEXPAND | wxALL, 5);
        sizer->Add(playBtn, 0, wxEXPAND | wxALL, 5);
        SetSizer(sizer);

        Bind(wxEVT_MENU, &Chip8Frame::OnOpenGame, this, wxID_FILE);
        Bind(wxEVT_MENU, &Chip8Frame::OnOpenFolder, this, wxID_OPEN);
        Bind(wxEVT_MENU, &Chip8Frame::OnQuit, this, wxID_EXIT);
        romList->Bind(wxEVT_LISTBOX, &Chip8Frame::OnSelectRom, this);
        romList->Bind(wxEVT_LISTBOX_DCLICK, &Chip8Frame::OnDoubleClickRom, this);
        playBtn->Bind(wxEVT_BUTTON, &Chip8Frame::OnRunGame, this);
    }

private:
    void OnOpenGame(wxCommandEvent &)
    {
        wxFileDialog dlg(this, "Open ROM", "", "",
                         "CHIP-8 ROMs (*.ch8;*.rom)|*.ch8;*.rom|All files (*.*)|*.*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK)
            OpenGame(dlg.GetPath());
    }

    void OnOpenFolder(wxCommandEvent &)
    {
        wxDirDialog dlg(this, "Choose a ROM folder");
        if (dlg.ShowModal() == wxID_OK)
        {
            selectedFolder = dlg.GetPath();
            romList->Clear();

            wxDir dir(selectedFolder);
            if (dir.IsOpened())
            {
                wxString filename;
                bool cont = dir.GetFirst(&filename, "*", wxDIR_FILES);
                while (cont)
                {
                    if (filename.EndsWith(".ch8") || filename.EndsWith(".rom"))
                        romList->Append(filename);
                    cont = dir.GetNext(&filename);
                }
            }
        }
    }

    void OnSelectRom(wxCommandEvent &) { playBtn->Enable(romList->GetSelection() != wxNOT_FOUND); }

    void OnDoubleClickRom(wxCommandEvent &)
    {
        int sel = romList->GetSelection();
        if (sel != wxNOT_FOUND)
            OpenGame(selectedFolder + wxFILE_SEP_PATH + romList->GetString(sel));
    }

    void OnRunGame(wxCommandEvent &)
    {
        int sel = romList->GetSelection();
        if (sel != wxNOT_FOUND)
            OpenGame(selectedFolder + wxFILE_SEP_PATH + romList->GetString(sel));
    }

    void OnQuit(wxCommandEvent &) { Close(); }

    void OpenGame(const wxString &path)
    {
        Chip8FrameWithCanvas *gameFrame = new Chip8FrameWithCanvas(chip8.get(), path);
        gameFrame->Show();
    }

    wxListBox *romList;
    wxButton *playBtn;
    wxString selectedFolder;
    std::unique_ptr<Chip8> chip8;
};

// -------------------------
// wxApp implementation
// -------------------------
bool Chip8App::OnInit()
{
    Chip8Frame *frame = new Chip8Frame();
    frame->Show(true);
    return true;
}

wxIMPLEMENT_APP(Chip8App);
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

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glBegin(GL_QUADS);
        for (int y = 0; y < 32; ++y)
        {
            for (int x = 0; x < 64; ++x)
            {
                if (chip8.gfx[y * 64 + x])
                {
                    glColor3f(1.0f, 1.0f, 1.0f);
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

// -------------------------
// Main frame (with canvas)
// -------------------------
class Chip8FrameWithCanvas : public wxFrame
{
public:
    Chip8FrameWithCanvas(Chip8 *chip8Ptr, const wxString &romFile)
        : wxFrame(nullptr, wxID_ANY, "CHIP-8 Emulator", wxDefaultPosition, wxSize(640, 320)),
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
        emulationMenu->AppendSubMenu(speedMenu, "Speed");

        wxMenuBar *menuBar = new wxMenuBar;
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(emulationMenu, "&Emulation");
        SetMenuBar(menuBar);

        // ---- Bind events ----
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnOpenGame, this, wxID_OPEN);
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnQuit, this, wxID_EXIT);
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnPause, this, wxID_STOP);
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnReset, this, wxID_REFRESH);
        Bind(wxEVT_MENU, &Chip8FrameWithCanvas::OnSpeedChange, this, ID_SPEED_FASTEST, ID_SPEED_SLOW);

        // ---- Canvas ----
        canvas = new Chip8Canvas(this, *chip8);
        // Set default speed
        canvas->emuSpeed = Chip8Canvas::Speed::Normal;
        speedMenu->Check(ID_SPEED_NORMAL, true); // sync menu selection

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

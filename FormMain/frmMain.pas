{
  ShutdownGuard
  Copyright (C) 2008  Stefan Sundin

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
}

unit frmMain;

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs, Menus, ShellAPI, ImgList, ExtCtrls;

const
  WM_ICONTRAY = WM_USER+1;

type
  TFormMain = class(TForm)
    PopupMenuTray: TPopupMenu;
    TrayMenuExit: TMenuItem;
    ImageListTrayIcon: TImageList;
    TrayMenuAsk: TMenuItem;
    TrayMenuState: TMenuItem;
    TrayMenuPrevent: TMenuItem;
    TrayMenuIdle: TMenuItem;
    TrayMenuSeperator2: TMenuItem;
    TrayMenuSeperator1: TMenuItem;
    TrayMenuAbout: TMenuItem;
    TrayMenuSeperator3: TMenuItem;
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure TrayMenuExitClick(Sender: TObject);
    procedure TrayMenuAskClick(Sender: TObject);
    procedure TrayMenuPreventClick(Sender: TObject);
    procedure TrayMenuIdleClick(Sender: TObject);
    procedure TrayMenuAboutClick(Sender: TObject);
  private
    { Private declarations }
//    procedure WMEndSession(var Msg: TWMEndSession); message WM_ENDSESSION;
    procedure WMQueryEndSession(var Msg: TWMQueryEndSession); message WM_QueryEndSession;
  public
    { Public declarations }
    TrayData: TNotifyIconData;
    procedure TrayMessage(var Msg: TMessage); message WM_ICONTRAY;

    procedure EnableStates;
    procedure SetState(iState: Integer);
    procedure ToggleState;
  end;

const
  _STATE_ASK = 0;
  _STATE_PREVENT = 1;
  _STATE_IDLE = 2;

var
  FormMain: TFormMain;
  _State: Integer = _STATE_ASK;
  {
   _State = 0 , Ask
   _State = 1 , Prevent
   _State = 2 , Idle
  }

implementation

{$R *.dfm}

procedure TFormMain.EnableStates;
begin
  //Ask
  TrayMenuAsk.Checked := False;
  TrayMenuAsk.Enabled := True;
  //Prevent
  TrayMenuPrevent.Checked := False;
  TrayMenuPrevent.Enabled := True;
  //Idle
  TrayMenuIdle.Checked := False;
  TrayMenuIdle.Enabled := True;
end;

procedure TFormMain.SetState(iState: Integer);
var
  StateIcon: TIcon;
begin
  _State := iState;
  EnableStates;
  if _State = _STATE_ASK then begin //Ask
    TrayMenuAsk.Checked := True;
    TrayMenuAsk.Enabled := False;
  end
  else if _State = _STATE_PREVENT then begin //Prevent
    TrayMenuPrevent.Checked := True;
    TrayMenuPrevent.Enabled := False;
  end
  else if _State = _STATE_IDLE then begin //Idle
    TrayMenuIdle.Checked := True;
    TrayMenuIdle.Enabled := False;
  end;
  StateIcon:=TIcon.Create;
  ImageListTrayIcon.GetIcon(_State,StateIcon);
  TrayData.hIcon:=StateIcon.Handle;
  Shell_NotifyIcon(NIM_MODIFY, @TrayData);
  StateIcon.Free;
end;

procedure TFormMain.ToggleState;
begin
  if _State = _STATE_ASK then SetState(_STATE_PREVENT)
  else if _State = _STATE_PREVENT then SetState(_STATE_IDLE)
  else if _State = _STATE_IDLE then SetState(_STATE_ASK);
end;

{
procedure TForm1.WMEndSession(var Msg: TWMEndSession);
begin
  ShowMessage('Windows is shutting me down :'+#39+'(');
  inherited;
end;
}

procedure TFormMain.WMQueryEndSession(var Msg: TWMQueryEndSession);
begin
  if _State = _STATE_ASK then begin
    FormMain.BringToFront;
    FormMain.Activate;
    if MessageDlg('Windows is shutting down/logging out, prevent?', mtConfirmation, [mbYes,mbNo], 0) = mrYes then
      Msg.Result := 0  //Yes, prevent
    else
      Msg.Result := 1;  //No, don't prevent
  end
  else if _State = _STATE_PREVENT then begin
    Msg.Result := 0;  //Yes, prevent
  end
  else if _State = _STATE_IDLE then begin
    Msg.Result := 1;  //No, don't prevent
  end;
end;

procedure TFormMain.TrayMessage(var Msg: TMessage);
var
  Pointer: TPoint;
begin
  case Msg.lParam of
    WM_LBUTTONDOWN:
      begin
        ToggleState;
      end;
    WM_RBUTTONDOWN:
      begin
        SetForegroundWindow(Handle);
        GetCursorPos(Pointer);
        PopupMenuTray.Popup(Pointer.X, Pointer.Y);
        PostMessage(Handle, WM_NULL, 0, 0);
      end;
  end;
end;

procedure TFormMain.FormCreate(Sender: TObject);
var
  i: Integer;
begin
  with TrayData do begin
    cbSize:=SizeOf(TrayData);
    Wnd:=FormMain.Handle;
    uID:=0;
    uFlags:=NIF_MESSAGE+NIF_ICON+NIF_TIP;
    uCallbackMessage:=WM_ICONTRAY;
    hIcon:=Application.Icon.Handle;  //Icon
    StrPCopy(szTip,  //Tray Tip
      Copy(  //Prevent Unhandled Exception if overwriting 63 chars in the Array
        Application.Title
      ,1,63)
    );
  end;
  Shell_NotifyIcon(NIM_ADD, @TrayData);

  {  Autostart switches (non-case-sensitive):
    -s, --state id
    id:
      Ask
      Prevent
      Idle
  }
  for i:=1 to ParamCount do begin
    if ((LowerCase(ParamStr(i)) = '-s') or (LowerCase(ParamStr(i)) = '--state')) then begin
      if ((LowerCase(ParamStr(i+1)) = 'ask') or (LowerCase(ParamStr(i+1)) = 'prevent') or (LowerCase(ParamStr(i+1)) = 'idle')) then begin
        if LowerCase(ParamStr(i+1)) = 'ask' then SetState(_STATE_ASK)
        else if LowerCase(ParamStr(i+1)) = 'prevent' then SetState(_STATE_PREVENT)
        else if LowerCase(ParamStr(i+1)) = 'idle' then SetState(_STATE_IDLE);
      end;
    end;
  end;


end;

procedure TFormMain.FormDestroy(Sender: TObject);
begin
  Shell_NotifyIcon(NIM_DELETE, @TrayData);
end;

procedure TFormMain.TrayMenuExitClick(Sender: TObject);
begin
  Application.Terminate;
end;

procedure TFormMain.TrayMenuAskClick(Sender: TObject);
begin
  SetState(_STATE_ASK);
end;

procedure TFormMain.TrayMenuPreventClick(Sender: TObject);
begin
  SetState(_STATE_PREVENT);
end;

procedure TFormMain.TrayMenuIdleClick(Sender: TObject);
begin
  SetState(_STATE_IDLE);
end;

procedure TFormMain.TrayMenuAboutClick(Sender: TObject);
begin
  ShowMessage(
    'ShutdownGuard'+#10#13+
    'http://shutdownguard.googlecode.com/'
  );
end;

end.

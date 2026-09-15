// Copyright(C) 2018-2020 by Steven Adler
//
// This file is part of TwoCan, a plugin for OpenCPN.
//
// TwoCan is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// TwoCan is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with TwoCan. If not, see <https://www.gnu.org/licenses/>.
//
// NMEA2000® is a registered Trademark of the National Marine Electronics Association


// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: Preferences Dialog for the Plugin
// Owner: twocanplugin@hotmail.com
// Date: 6/8/2018
// Version: 1.0
// 1.3 - Added Support for Linux (socketCAN interfaces)
// 1.4 - Implemented support for active mode, participate on NMEA 2000 network
// 1.5 - 10/7/2019. Flags for XTE, Attitude, Additional log formats
// 1.6 - 10/10/2019 Flags for Rudder, Engine and Fluid levels
// 1.7 - 10/12/2019 Flags for Battery
// 1.8 - 10/05/2020 AIS data validation fixes, Mac OSX support
// 1.9 - 20/08/2020 Rusoku adapter support on Mac OSX, OCPN 5.2 Plugin Manager support
// 2.0 - 04/07/2021 Bi-directional gateway, PCAP log files
// 2.1 - 20/05/2022 Add configuration items for Media Player, Waypoint Creation and Autopilot (not yet implemented)
// Outstanding Features: 
// 1. Prevent selection of driver that is not physically present
// 2. Prevent user selecting both LogFile reader and Log Raw frames !
//

#include "twocansettings.h"

#if defined (__WXMSW__)
#include <wx/combobox.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>

#include <algorithm>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace {

const wxString kCanableAutoLabel = _T("Automatic (detect CANable V2.0)");
const wxString kCanableComboName = _T("TwoCanCanablePort");
const wxString kCanableStatusName = _T("TwoCanCanableStatus");
const wxString kCanableRefreshName = _T("TwoCanCanableRefresh");
const wchar_t kCanableVidPid[] = L"VID_16D0&PID_117E";
const wchar_t kCanableRegistryKey[] = L"Software\\TwoCan\\CANable";
const wchar_t kCanableRegistryValue[] = L"ComPort";
const wchar_t kCanableEnvironment[] = L"TWOCAN_CANABLE_COM";

struct ComPortEntry {
	wxString port;
	wxString friendlyName;
	bool canable;
};

wxString NormalizeComPortName(wxString value) {
	value.Trim(true);
	value.Trim(false);
	value.MakeUpper();

	if (value.StartsWith(_T("\\\\.\\"))) {
		value = value.Mid(4);
	}
	if (value.EndsWith(_T(":"))) {
		value.RemoveLast();
	}
	if (!value.StartsWith(_T("COM")) || value.Length() < 4) {
		return wxEmptyString;
	}

	unsigned long portNumber = 0;
	if (!value.Mid(3).ToULong(&portNumber) || portNumber == 0 || portNumber > 4096) {
		return wxEmptyString;
	}
	return wxString::Format(_T("COM%lu"), portNumber);
}

unsigned long ComPortNumber(const wxString& port) {
	unsigned long result = 0;
	NormalizeComPortName(port).Mid(3).ToULong(&result);
	return result;
}

bool MultiSzContainsCanable(const wchar_t* values) {
	if (!values) return false;
	for (const wchar_t* p = values; *p; p += wcslen(p) + 1) {
		wxString id(p);
		id.MakeUpper();
		if (id.Contains(kCanableVidPid)) return true;
	}
	return false;
}

bool ReadPortName(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA* deviceInfo, wxString* port) {
	if (!port) return false;
	HKEY key = SetupDiOpenDevRegKey(deviceInfoSet, deviceInfo, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
	if (key == INVALID_HANDLE_VALUE) return false;

	wchar_t value[128] = {};
	DWORD type = 0;
	DWORD size = sizeof(value);
	const LONG rc = RegQueryValueExW(key, L"PortName", nullptr, &type,
		reinterpret_cast<LPBYTE>(value), &size);
	RegCloseKey(key);
	if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return false;

	const wxString normalized = NormalizeComPortName(wxString(value));
	if (normalized.IsEmpty()) return false;
	*port = normalized;
	return true;
}

wxString ReadFriendlyName(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA* deviceInfo) {
	wchar_t value[512] = {};
	DWORD type = 0;
	DWORD required = 0;
	if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, deviceInfo, SPDRP_FRIENDLYNAME,
		&type, reinterpret_cast<PBYTE>(value), sizeof(value), &required)) {
		return wxString(value);
	}
	if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, deviceInfo, SPDRP_DEVICEDESC,
		&type, reinterpret_cast<PBYTE>(value), sizeof(value), &required)) {
		return wxString(value);
	}
	return wxEmptyString;
}

std::vector<ComPortEntry> EnumerateComPorts() {
	std::vector<ComPortEntry> result;
	HDEVINFO devices = SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
	if (devices == INVALID_HANDLE_VALUE) return result;

	SP_DEVINFO_DATA deviceInfo = {};
	deviceInfo.cbSize = sizeof(deviceInfo);
	for (DWORD index = 0; SetupDiEnumDeviceInfo(devices, index, &deviceInfo); ++index) {
		wxString port;
		if (!ReadPortName(devices, &deviceInfo, &port)) continue;

		wchar_t hardwareIds[4096] = {};
		DWORD type = 0;
		DWORD required = 0;
		bool isCanable = false;
		if (SetupDiGetDeviceRegistryPropertyW(devices, &deviceInfo, SPDRP_HARDWAREID,
			&type, reinterpret_cast<PBYTE>(hardwareIds), sizeof(hardwareIds), &required)) {
			isCanable = MultiSzContainsCanable(hardwareIds);
		}

		ComPortEntry entry;
		entry.port = port;
		entry.friendlyName = ReadFriendlyName(devices, &deviceInfo);
		entry.canable = isCanable;
		result.push_back(entry);
	}
	SetupDiDestroyDeviceInfoList(devices);

	std::sort(result.begin(), result.end(), [](const ComPortEntry& a, const ComPortEntry& b) {
		return ComPortNumber(a.port) < ComPortNumber(b.port);
	});
	return result;
}

wxString CanableIniPath() {
	return pluginDataFolder + _T("drivers") + wxFileName::GetPathSeparator() + _T("cantact.ini");
}

wxString ReadPersistedCanablePort() {
	wchar_t value[128] = {};
	DWORD count = GetEnvironmentVariableW(kCanableEnvironment, value, _countof(value));
	if (count > 0 && count < _countof(value)) {
		const wxString normalized = NormalizeComPortName(wxString(value));
		if (!normalized.IsEmpty()) return normalized;
	}

	const wxString iniPath = CanableIniPath();
	value[0] = 0;
	GetPrivateProfileStringW(L"CANable", L"ComPort", L"", value, _countof(value), iniPath.wc_str());
	wxString normalized = NormalizeComPortName(wxString(value));
	if (!normalized.IsEmpty()) return normalized;

	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, kCanableRegistryKey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
		DWORD type = 0;
		DWORD size = sizeof(value);
		value[0] = 0;
		if (RegQueryValueExW(key, kCanableRegistryValue, nullptr, &type,
			reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS && type == REG_SZ) {
			normalized = NormalizeComPortName(wxString(value));
		}
		RegCloseKey(key);
		if (!normalized.IsEmpty()) return normalized;
	}
	return wxEmptyString;
}

wxComboBox* FindCanableCombo(wxWindow* root) {
	return wxDynamicCast(wxWindow::FindWindowByName(kCanableComboName, root), wxComboBox);
}

wxStaticText* FindCanableStatus(wxWindow* root) {
	return wxDynamicCast(wxWindow::FindWindowByName(kCanableStatusName, root), wxStaticText);
}

wxButton* FindCanableRefresh(wxWindow* root) {
	return wxDynamicCast(wxWindow::FindWindowByName(kCanableRefreshName, root), wxButton);
}

wxString PortFromComboValue(wxString value) {
	value.Trim(true);
	value.Trim(false);
	if (value.StartsWith(_T("Automatic"))) return wxEmptyString;

	const int separator = value.Find(_T(' '));
	if (separator != wxNOT_FOUND) value = value.Left(separator);
	return NormalizeComPortName(value);
}

void PopulateCanablePorts(wxComboBox* combo, wxStaticText* status) {
	if (!combo || !status) return;
	const wxString configured = ReadPersistedCanablePort();
	const std::vector<ComPortEntry> ports = EnumerateComPorts();

	combo->Freeze();
	combo->Clear();
	combo->Append(kCanableAutoLabel);

	wxArrayString detectedCanable;
	wxString configuredLabel;
	for (const auto& entry : ports) {
		wxString label = entry.port;
		if (!entry.friendlyName.IsEmpty()) {
			label += _T(" — ") + entry.friendlyName;
		}
		if (entry.canable) {
			label += _T(" [CANable V2.0 16D0:117E]");
			detectedCanable.Add(entry.port);
		}
		combo->Append(label);
		if (!configured.IsEmpty() && configured.CmpNoCase(entry.port) == 0) configuredLabel = label;
	}

	if (configured.IsEmpty()) {
		combo->SetValue(kCanableAutoLabel);
	} else if (!configuredLabel.IsEmpty()) {
		combo->SetValue(configuredLabel);
	} else {
		// Keep an unavailable/manual COM visible and editable.
		combo->SetValue(configured);
	}
	combo->Thaw();

	if (!detectedCanable.IsEmpty()) {
		status->SetLabel(_T("CANable V2.0 detected automatically on: ") + wxJoin(detectedCanable, ','));
	} else {
		status->SetLabel(_T("CANable V2.0 (16D0:117E) not detected. Choose or type a COM port manually."));
	}
	status->Wrap(500);
}

bool IsCanableInterface(const wxString& name) {
	wxString lower = name.Lower();
	return lower.Contains(_T("canable")) || lower.Contains(_T("cantact"));
}

void UpdateCanableUiEnabled(wxWindow* root, const wxString& interfaceName) {
	const bool enabled = IsCanableInterface(interfaceName);
	if (FindCanableCombo(root)) FindCanableCombo(root)->Enable(enabled);
	if (FindCanableRefresh(root)) FindCanableRefresh(root)->Enable(enabled);
	if (FindCanableStatus(root)) FindCanableStatus(root)->Enable(enabled);
}

bool PersistCanableSelection(wxWindow* root) {
	wxComboBox* combo = FindCanableCombo(root);
	if (!combo) return true;

	const wxString raw = combo->GetValue();
	const bool automatic = raw.StartsWith(_T("Automatic"));
	const wxString port = automatic ? wxEmptyString : PortFromComboValue(raw);
	if (!automatic && port.IsEmpty()) {
		wxMessageBox(_T("Enter a valid Windows COM port, for example COM7 or COM12."),
			_T("CANable port"), wxOK | wxICON_WARNING, root);
		return false;
	}

	const wxString iniPath = CanableIniPath();
	if (automatic) {
		SetEnvironmentVariableW(kCanableEnvironment, nullptr);
		WritePrivateProfileStringW(L"CANable", L"ComPort", L"", iniPath.wc_str());

		HKEY key = nullptr;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, kCanableRegistryKey, 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
			RegDeleteValueW(key, kCanableRegistryValue);
			RegCloseKey(key);
		}
		wxLogMessage(_T("TwoCan Settings, CANable port set to automatic VID/PID detection"));
	} else {
		SetEnvironmentVariableW(kCanableEnvironment, port.wc_str());
		if (!WritePrivateProfileStringW(L"CANable", L"ComPort", port.wc_str(), iniPath.wc_str())) {
			wxLogError(_T("TwoCan Settings, unable to save CANable COM port to %s"), iniPath);
			wxMessageBox(_T("Unable to save the CANable COM-port setting."),
				_T("CANable port"), wxOK | wxICON_ERROR, root);
			return false;
		}

		HKEY key = nullptr;
		if (RegCreateKeyExW(HKEY_CURRENT_USER, kCanableRegistryKey, 0, nullptr, 0,
			KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
			const DWORD bytes = static_cast<DWORD>((port.Length() + 1) * sizeof(wchar_t));
			RegSetValueExW(key, kCanableRegistryValue, 0, REG_SZ,
				reinterpret_cast<const BYTE*>(port.wc_str()), bytes);
			RegCloseKey(key);
		}
		wxLogMessage(_T("TwoCan Settings, CANable manual port set to %s"), port);
	}
	return true;
}

void AddCanablePortControls(wxPanel* panelSettings) {
	if (!panelSettings || !panelSettings->GetSizer()) return;

	wxStaticBoxSizer* portSizer = new wxStaticBoxSizer(
		new wxStaticBox(panelSettings, wxID_ANY, _T("CANable V2.0 serial port")), wxVERTICAL);
	wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);

	wxComboBox* combo = new wxComboBox(portSizer->GetStaticBox(), wxID_ANY, wxEmptyString,
		wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN);
	combo->SetName(kCanableComboName);
	combo->SetMinSize(wxSize(350, -1));
	row->Add(combo, 1, wxALL | wxEXPAND, 5);

	wxButton* refresh = new wxButton(portSizer->GetStaticBox(), wxID_ANY, _T("Refresh"));
	refresh->SetName(kCanableRefreshName);
	row->Add(refresh, 0, wxALL, 5);
	portSizer->Add(row, 0, wxEXPAND, 5);

	wxStaticText* status = new wxStaticText(portSizer->GetStaticBox(), wxID_ANY, wxEmptyString);
	status->SetName(kCanableStatusName);
	portSizer->Add(status, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// Insert directly below the existing NMEA 2000 Interfaces selector.
	panelSettings->GetSizer()->Insert(1, portSizer, 0, wxEXPAND, 5);

	refresh->Bind(wxEVT_BUTTON, [combo, status](wxCommandEvent&) {
		PopulateCanablePorts(combo, status);
	});
	PopulateCanablePorts(combo, status);
}

} // namespace
#endif

// Constructor and destructor implementation
// inherits froms TwoCanSettingsBase which was implemented using wxFormBuilder
TwoCanSettings::TwoCanSettings(wxWindow* parent, wxWindowID id, const wxString& title, \
	const wxPoint& pos, const wxSize& size, long style )
	: TwoCanSettingsBase(parent, id, title, pos, size, style) {

	parentWindow = parent;

	// Set the dialog's 16x16 icon
	wxIcon icon;
	icon.CopyFromBitmap(*_img_Toucan_16);
	TwoCanSettings::SetIcon(icon);
	togglePGN = FALSE;

#if defined (__WXMSW__)
	AddCanablePortControls(panelSettings);
#endif
}

TwoCanSettings::~TwoCanSettings() {
// Just in case we are closed from the dialog's close button 
	
	// We are closing...
	debugWindowActive = FALSE;

	// Clear the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->Clear();
		wxTheClipboard->Close();
	}
}

void TwoCanSettings::OnInit(wxInitDialogEvent& event) {
	this->settingsDirty = FALSE;
		
	// Settings Tab
	wxArrayString *pgn = new wxArrayString();

	// BUG BUG Localization
	// Note to self, order must match FLAGS
	pgn->Add(_T("127250 ") + _("Heading") + _T(" (HDG)"));
	pgn->Add(_T("128259 ") + _("Speed") + _T(" (VHW)"));
	pgn->Add(_T("128267 ") + _("Depth") + _T(" (DPT)"));
	pgn->Add(_T("129025 ") + _("Position") + _T(" (GLL)"));
	pgn->Add(_T("129026 ") + _("Course and Speed over Ground") + _(" (VTG)"));
	pgn->Add(_T("129029 ") + _("GNSS") + _T(" (GGA)"));
	pgn->Add(_T("129033 ") + _("Time") + _T(" (ZDA)"));
	pgn->Add(_T("130306 ") + _("Wind") + _T(" (MWV)"));
	pgn->Add(_T("130310 ") + _("Water Temperature") + _(" (MWT)"));
	pgn->Add(_T("129808 ") + _("Digital Selective Calling") + _T(" (DSC)"));
	pgn->Add(_T("129038..41 ") + _("AIS Class A & B messages") + _T(" (VDM)"));
	pgn->Add(_T("129285 ") + _("Route/Waypoint") + _T(" (BWR/BOD/WPL/RTE)"));
	pgn->Add(_T("127251 ") + _("Rate of Turn") + _T(" (ROT)"));
	pgn->Add(_T("129283 ") + _("Cross Track Error") + _T(" (XTE)"));
	pgn->Add(_T("127257 ") + _("Attitude") + _T(" (XDR)"));
	pgn->Add(_T("127488..49 ") + _("Engine Parameters") + _T(" (XDR)"));
	pgn->Add(_T("127505 ") + _("Fluid Levels") + _T(" (XDR) "));
	pgn->Add(_T("127245 ") + _("Rudder Angle") + _T(" (RSA)"));
	pgn->Add(_T("127508 ") + _("Battery Status") + _T(" (XDR)"));
	pgn->Add(_T("129284 ") + _("Navigation Data") + _T(" (BWC/BWR/BOD/WCV)"));
	pgn->Add(_T("128275 ") + _("Vessel Trip Details") +_T(" (VLW)"));
	pgn->Add(_T("130323 ") + _("Meteorological Details") +_T(" (MDA)"));
	pgn->Add(_T("127233 ") + _("Man Overboard") + _T(" (MOB)"));
	
	// Populate the listbox and check/uncheck as appropriate
	for (size_t i = 0; i < pgn->Count(); i++) {
		chkListPGN->Append(pgn->Item(i));
		chkListPGN->Check(i, (supportedPGN & (int)pow(2, i) ? TRUE : FALSE));
	}

	// Search for the twocan plugin drivers that are present
	EnumerateDrivers();
	
	// Populate the ComboBox and set the default driver selection
	for (AvailableAdapters::iterator it = this->adapters.begin(); it != this->adapters.end(); it++){
		// For Windows, the first item of the hashmap is the "friendly name", the second is the full path of the driver
		// For Linux, both the first & second item of the hashmap is either "Log File Reader" or can adapter name, eg. "can0"
		cmbInterfaces->Append(it->first);
		// Ensure that the driver being used is selected in the Combobox 
		if (canAdapter == it->second) {
			cmbInterfaces->SetStringSelection(it->first);
		}
	}

#if defined (__WXMSW__)
	UpdateCanableUiEnabled(this, cmbInterfaces->GetStringSelection());
#endif
	
	// About Tab
	bmpAbout->SetBitmap(wxBitmap(*_img_Toucan_64));
  
	// BUG BUG Localization & version numbers
	txtAbout->SetLabel(_T("TwoCan PlugIn for OpenCPN\nEnables some NMEA2000\xae data to be directly integrated with OpenCPN.\nSend bug reports to twocanplugin@hotmail.com"));
	txtAbout->Wrap(512);

	// Debug Tab
	// BUG BUG Localization
	btnPause->SetLabel((debugWindowActive) ? _("Stop") : _("Start"));

	// Network Tab
	
	for (int i = 0; i < CONST_MAX_DEVICES; i++) {
		// Renumber row labels to match network address 0 - 253
		dataGridNetwork->SetRowLabelValue(i, std::to_string(i));
		// No need to iterate over non-existent entries
		if ((networkMap[i].uniqueId > 0) || (strlen(networkMap[i].productInformation.modelId) > 0) ) {
			dataGridNetwork->SetCellValue(i, 0, wxString::Format("%lu", networkMap[i].uniqueId));
			// Look up the manufacturer name
			std::unordered_map<int, std::string>::iterator it = deviceManufacturers.find(networkMap[i].manufacturerId);
			if (it != deviceManufacturers.end()) {
				dataGridNetwork->SetCellValue(i, 1, it->second);
			}
			else {
				dataGridNetwork->SetCellValue(i, 1, wxString::Format("%d", networkMap[i].manufacturerId));
			}
			dataGridNetwork->SetCellValue(i, 2, wxString::Format("%s", networkMap[i].productInformation.modelId));
			// We don't receive our own heartbeats so ignore our time stamp value
			if (networkMap[i].uniqueId != uniqueId) {
				wxGridCellAttr *attr;
				attr = new wxGridCellAttr;
				// Differentiate dead/alive devices 
				attr->SetTextColour((wxDateTime::Now() > (networkMap[i].timestamp + wxTimeSpan::Seconds(60))) ? *wxRED : *wxGREEN);
				dataGridNetwork->SetAttr(i, 0, attr);
			}
		}
	}
	
	// Device tab
	chkDeviceMode->SetValue(deviceMode);
	chkHeartbeat->Enable(chkDeviceMode->IsChecked());
	chkGateway->Enable(chkDeviceMode->IsChecked());
	chkWaypoint->Enable(chkDeviceMode->IsChecked());
	chkMedia->Enable(chkDeviceMode->IsChecked());
	if (deviceMode == TRUE) {
		chkHeartbeat->SetValue(enableHeartbeat);
		chkGateway->SetValue(enableGateway);
		chkWaypoint->SetValue(enableWaypoint);
		chkMedia->SetValue(enableMusic);
	}
	else {
		chkHeartbeat->SetValue(FALSE);
		chkGateway->SetValue(FALSE);
		chkWaypoint->SetValue(FALSE);
		chkMedia->SetValue(FALSE);
	}

	labelNetworkAddress->SetLabel(wxString::Format("Network Address: %u", networkAddress));
	labelUniqueId->SetLabel(wxString::Format("Unique ID: %lu", uniqueId));
	labelModelId->SetLabel(wxString::Format("Model ID: %s", PLUGIN_COMMON_NAME));
	labelManufacturer->SetLabel("Manufacturer: TwoCan");
	labelSoftwareVersion->SetLabel(wxString::Format("Software Version: %d.%d.%d", PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH));
	labelDevice->SetLabel(wxString::Format("Device Class: %d", CONST_DEVICE_CLASS));
	labelFunction->SetLabel(wxString::Format("Device Function: %d", CONST_DEVICE_FUNCTION));

	// Logging Options
	// Add Logging Options to the hashmap
	logging["None"] = FLAGS_LOG_NONE;
	logging["TwoCan"] = FLAGS_LOG_RAW;
	logging["Canboat"] = FLAGS_LOG_CANBOAT;
	logging["Candump"] = FLAGS_LOG_CANDUMP;
	logging["YachtDevices"] = FLAGS_LOG_YACHTDEVICES;
	logging["CSV"] = FLAGS_LOG_CSV;

	for (LoggingOptions::iterator it = this->logging.begin(); it != this->logging.end(); it++){
		cmbLogging->Append(it->first);
		if (logLevel == it->second) {
			cmbLogging->SetStringSelection(it->first);
		}
	}

	// Autopilot Settings
	rdoBoxAutopilot->SetSelection(autopilotModel);

	// BUG BUG I really don't understand wxWidgets sizers, but this seems to do what I want
	wxSize newSize = this->GetSize();
	dataGridNetwork->SetMinSize(wxSize(512, 20 * dataGridNetwork->GetDefaultRowSize()));
	dataGridNetwork->SetMaxSize(wxSize(-1, 20 * dataGridNetwork->GetDefaultRowSize()));
		
	Fit();

	// After we've fitted in everything adjust the dataGrid column widths
	int colWidth = (int)((dataGridNetwork->GetSize().GetWidth() - dataGridNetwork->GetRowLabelSize() - wxSystemSettings::GetMetric(wxSYS_VSCROLL_X, NULL)) / 3);
	dataGridNetwork->SetColSize(0, colWidth);
	dataGridNetwork->SetColSize(1, colWidth);
	dataGridNetwork->SetColSize(2, colWidth);
	
}

// BUG BUG Should prevent the user from shooting themselves in the foot if they select a driver that is not present
void TwoCanSettings::OnChoiceInterfaces(wxCommandEvent &event) {
	// BUG BUG should only set the dirty flag if we've actually selected a different driver
	this->settingsDirty = TRUE;
#if defined (__WXMSW__)
	UpdateCanableUiEnabled(this, cmbInterfaces->GetStringSelection());
#endif
}

// Select NMEA 2000 parameter group numbers to be converted to their respective NMEA 0183 sentences
void TwoCanSettings::OnCheckPGN(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
} 

// Enable Logging of Raw NMEA 2000 frames
void TwoCanSettings::OnChoiceLogging(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Toggle real time display of received NMEA 2000 frames
void TwoCanSettings::OnPause(wxCommandEvent &event) {
	debugWindowActive = !debugWindowActive;
	// BUG BUG Localization
	TwoCanSettings::btnPause->SetLabel((debugWindowActive) ? _("Stop") : _("Start"));
}

// Copy the text box contents to the clipboard
void TwoCanSettings::OnCopy(wxCommandEvent &event) {
	if (wxTheClipboard->Open()) {
		wxTheClipboard->SetData(new wxTextDataObject(txtDebug->GetValue()));
		wxTheClipboard->Close();
	}
}

void TwoCanSettings::OnExportWaypoint(wxCommandEvent &event) {
	wxMessageBox("Export Waypoint Settings Dialog");
}

// Set whether the device is an actve or passive node on the NMEA 2000 network
void TwoCanSettings::OnCheckMode(wxCommandEvent &event) {
	chkHeartbeat->Enable(chkDeviceMode->IsChecked());
	chkHeartbeat->SetValue(enableHeartbeat);
	chkGateway->Enable(chkDeviceMode->IsChecked());
	chkGateway->SetValue(enableGateway);
	chkWaypoint->Enable(chkDeviceMode->IsChecked());
	chkWaypoint->SetValue(enableWaypoint);
	// BUG BUG Not yet implemented
	// chkMedia->Enable(chkDeviceMode->IsChecked());
	// chkMedia->SetValue(enableMusic);
	// chkAutopilot->Enable(chkDeviceMode->IsChecked());
	// chkAutopilot->SetValue(enableAutopilot);
	this->settingsDirty = TRUE;
}

// Set whether the device sends heartbeats onto the network
void TwoCanSettings::OnCheckHeartbeat(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device acts as a bi-directional gateway, NMEA 183 -> NMEA 2000
void TwoCanSettings::OnCheckGateway(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device integrates with Fusion Media players
void TwoCanSettings::OnCheckMedia(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device will create an OpenCPN waypoint on reception of PGN 130074
void TwoCanSettings::OnCheckWaypoint(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set autopilot model
void TwoCanSettings::OnAutopilotModelChanged(wxCommandEvent& event) {
	this->settingsDirty = TRUE;
}

// Right mouse click to check/uncheck all parameter group numbers
void TwoCanSettings::OnRightClick(wxMouseEvent& event) {
	togglePGN = !togglePGN;
	for (unsigned int i = 0; i < chkListPGN->GetCount(); i++) {
		chkListPGN->Check(i, togglePGN);
	}
	this->settingsDirty = TRUE;
}


void TwoCanSettings::OnOK(wxCommandEvent &event) {
	// Disable receiving of NMEA 2000 frames in the debug window, as we'll be closing
	debugWindowActive = FALSE;

#if defined (__WXMSW__)
	if (IsCanableInterface(cmbInterfaces->GetStringSelection()) && !PersistCanableSelection(this)) {
		return;
	}
#endif

	// Save the settings
	if (this->settingsDirty) {
		SaveSettings();
		this->settingsDirty = FALSE;
	}

	// Clear the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->Clear();
		wxTheClipboard->Close();
	}
	
	// Return OK
	EndModal(wxID_OK);
}

void TwoCanSettings::OnApply(wxCommandEvent &event) {
#if defined (__WXMSW__)
	if (IsCanableInterface(cmbInterfaces->GetStringSelection()) && !PersistCanableSelection(this)) {
		return;
	}
#endif
	// Save the settings
	if (this->settingsDirty) {
		SaveSettings();
		this->settingsDirty = FALSE;
	}
}

void TwoCanSettings::OnCancel(wxCommandEvent &event) {
	// Disable receiving of NMEA 2000 frames in the debug window, as we'll be closing
	debugWindowActive = FALSE;

	// Clear the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->Clear();
		wxTheClipboard->Close();
	}

	// Ignore any changed settings and return CANCEL
	EndModal(wxID_CANCEL);
}

void TwoCanSettings::SaveSettings(void) {
	// Enumerate the check list box to determine checked items
	wxArrayInt checkedItems;
	chkListPGN->GetCheckedItems(checkedItems);

	supportedPGN = 0;
	// Save the bitflags representing the checked items
	for (wxArrayInt::const_iterator it = checkedItems.begin(); it < checkedItems.end(); it++) {
		supportedPGN |= 1 << (int)*it;
	}

	enableHeartbeat = FALSE;
	if (chkHeartbeat->IsChecked()) {
		enableHeartbeat = TRUE;
	}

	enableGateway = FALSE;
	if (chkGateway->IsChecked()) {
		enableGateway = TRUE;
	}
		
	deviceMode = FALSE;
	if (chkDeviceMode->IsChecked()) {
		deviceMode = TRUE;
	}

	enableMusic = FALSE;
	if (chkMedia->IsChecked()) {
		enableMusic = TRUE;
	}

	enableWaypoint = FALSE;
	if (chkWaypoint->IsChecked()) {
		enableWaypoint = TRUE;
	}

	autopilotModel = rdoBoxAutopilot->GetSelection();

	if (cmbInterfaces->GetSelection() != wxNOT_FOUND) {
		canAdapter = adapters[cmbInterfaces->GetStringSelection()];
	} 
	else {
		canAdapter = _T("None");
	}

	logLevel = FLAGS_LOG_NONE;
	if (cmbLogging->GetSelection() != wxNOT_FOUND) {
		logLevel = logging[cmbLogging->GetStringSelection()];
	}
	else {
		logLevel = FLAGS_LOG_NONE;
	}
}

// Lists the available CAN bus or Logfile interfaces 
bool TwoCanSettings::EnumerateDrivers(void) {
	
#if defined (__WXMSW__)
wxString driversFolder = pluginDataFolder +  _T("drivers") + wxFileName::GetPathSeparator();

	// BUG BUG Should we log this ?
	wxLogMessage(_T("TwoCan Settings, Driver Path: %s"), driversFolder);

	wxDir adapterDirectory;

	if (adapterDirectory.Exists(driversFolder)) {
		adapterDirectory.Open(driversFolder);
	
		wxString fileName;
		wxString fileSpec = wxT("*.dll");
		
		bool foundFile = adapterDirectory.GetFirst(&fileName, fileSpec, wxDIR_FILES);
		while (foundFile){

			// Construct full path to selected driver and fetch driver information
			GetDriverInfo(wxString::Format("%s%s", driversFolder, fileName));

			foundFile = adapterDirectory.GetNext(&fileName);
		} 
	}
	else {
		// BUG BUG Should we log this ??
		wxLogMessage(_T("TwoCan Settings, driver folder not found"));
	}
	
#endif
	
#if defined (__LINUX__)
	// Add the built-in Log File Reader to the Adapter hashmap
	// BUG BUG Should add a #define for this string constant
	adapters["Log File Reader"] = "Log File Reader";
	adapters["Pcap File Reader"] = "Pcap File Reader";
	// Add any physical CAN Adapters
	std::vector<wxString> canAdapters;
	// Enumerate installed CAN adapters
	canAdapters = TwoCanSocket::ListCanInterfaces();
	if (canAdapters.size() == 0) {
		// Log that no CAN interfaces exist
		wxLogMessage(_T("TwoCan Settings, No CAN interface present"));
	}
	else {
		//wxLogMessage(_T("TwoCan CAN Interface Socket: %i"),socketDescriptor);
		// Retrieve the name of the interface, by using the index.
		// BUG BUG Why doesn't the socketDescriptor remain constant
		// interfaceRequest.ifr_ifindex = 3; //socketDescriptor;
		// returnCode = ioctl(socketDescriptor, SIOCGIFNAME, &interfaceRequest);
		// wxLogMessage(_T("TwoCan, Get CAN Interface result: %d"),returnCode);
		// if (returnCode != -1)  {
		//	wxLogMessage(_T("TwoCan, CAN Adapter Name: %s"),interfaceRequest.ifr_name);
		//	adapters[interfaceRequest.ifr_name] = interfaceRequest.ifr_name;
		for (auto it = canAdapters.begin(); it != canAdapters.end(); ++it) {
			wxLogMessage(_T("TwoCan Settings, Found CAN adapter: %s"),*it);
			adapters[*it] = *it;
		}
		
	}
	
#endif

#if defined (__APPLE__) && defined (__MACH__)
	// Add the built-in Log File Reader, Pcap file reader, Cantact, Kvaser and Rusoku interfaces to the Adapter hashmap
	adapters["Log File Reader"] = "Log File Reader";
	adapters["Pcap File Reader"] = "Pcap File Reader";
	adapters["Cantact"] = "Cantact";
	adapters["Kvaser"] = "Kvaser";
	adapters["Rusoku"] = "Rusoku";
#endif


	return TRUE;
}

#if defined (__WXMSW__) 

// Retrieve the human friendly name for the different Windows drivers to populate the combo box
void TwoCanSettings::GetDriverInfo(wxString fileName) {
	HMODULE dllHandle;
	LPFNDLLDriverName driverName = NULL;
	
	// BUG BUG Log this
	 wxLogMessage(_T("TwoCan Settings, Attempting to load driver: %s"), fileName);

	// Get a handle to the DLL module.
	dllHandle = LoadLibrary(fileName);
	
	// If the handle is valid, try to get the function addresses. 
	if (dllHandle != NULL)	{

		// Get pointer to driverName function
		driverName = (LPFNDLLDriverName)GetProcAddress(dllHandle, "DriverName");

		// If the function address is valid, call the function. 
		if (driverName != NULL)	{
			
			// Add entry to the Adapter hashmap
			adapters[driverName()] = fileName;
		}
		else {

			// BUG BUG Log an error indicating a DLL in the driver directory does not support the expected methods
			wxLogError(_T("TwoCan Settings, Invalid driverName function %d for %s"), GetLastError(), fileName);
		}

		// Free the DLL module.
		BOOL freeResult = FreeLibrary(dllHandle);
	}
	else {

		// BUG BUG Should log this
		wxLogError(_T("TwoCan Settings, Invalid DLL Handle Error: %d for %s"), GetLastError(), fileName);
	}
}

#endif


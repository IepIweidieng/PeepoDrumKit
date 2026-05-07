#include "core_io.h"
#include "core_string.h"
#include <vector>
#include <algorithm>
#include <shlwapi.h>
#include <shobjidl.h>
#include <Windows.h>
#include <wrl.h>
#include <filesystem>
using Microsoft::WRL::ComPtr;
using std::filesystem::u8path;

#define CHECK_EC(_ec) check_ec(_ec, __FILE__, __LINE__, __func__)
static b8 check_ec(const std::error_code& ec, std::string_view file, i32 line, std::string_view func)
{
	if (ec) {
		auto msg = ec.message();
		printf("%.*s:%d, in %.*s: %.*s", FmtStrViewArgs(file), line, FmtStrViewArgs(func), FmtStrViewArgs(msg));
	}
	return b8{ ec };
}

namespace Path
{
	static constexpr std::string_view Win32RootDirectoryPrefix = "\\";
	static constexpr std::string_view Win32CurrentDirectoryPrefix = ".\\";
	static constexpr std::string_view Win32ParentDirectoryPrefix = "..\\";

	std::string_view GetExtension(std::string_view filePath)
	{
		const size_t lastDirectoryIndex = filePath.find_last_of(DirectorySeparators);
		const std::string_view directoryTrimmed = (lastDirectoryIndex == std::string_view::npos) ? filePath : filePath.substr(lastDirectoryIndex);

		const size_t lastExtensionIndex = directoryTrimmed.find_last_of(ExtensionSeparator);
		return (lastExtensionIndex == std::string_view::npos) ? "" : directoryTrimmed.substr(lastExtensionIndex);
	}

	std::string_view TrimExtension(std::string_view filePath)
	{
		const std::string_view filePathExtension = GetExtension(filePath);
		return filePath.substr(0, filePath.size() - filePathExtension.size());
	}

	b8 HasExtension(std::string_view filePath, std::string_view extension)
	{
		const std::string_view filePathExtension = GetExtension(filePath);
		return ASCII::MatchesInsensitive(filePathExtension, extension);
	}

	b8 HasAnyExtension(std::string_view filePath, std::string_view packedExtensions)
	{
		const std::string_view filePathExtension = GetExtension(filePath);
		if (filePathExtension.empty() || packedExtensions.empty())
			return false;

		b8 anyExtensionMatches = false;
		ASCII::ForEachInCharSeparatedList(packedExtensions, ';', [&](std::string_view extension)
		{
			if (extension.empty()) { assert(!"Accidental invalid packedExtensions format (?)"); return; }
			if (ASCII::MatchesInsensitive(filePathExtension, extension))
				anyExtensionMatches = true;
		});
		return anyExtensionMatches;
	}

	std::string_view GetFileName(std::string_view filePath, b8 includeExtension)
	{
		const size_t lastDirectoryIndex = filePath.find_last_of(DirectorySeparators);
		const std::string_view fileName = (lastDirectoryIndex == std::string_view::npos) ? filePath : filePath.substr(lastDirectoryIndex + 1);
		return (includeExtension) ? fileName : TrimExtension(fileName);
	}

	std::string_view GetDirectoryName(std::string_view filePath)
	{
		const size_t lastDirectoryIndex = filePath.find_last_of(DirectorySeparators);
		return (lastDirectoryIndex == std::string_view::npos) ? ""
			: filePath.substr(0, (lastDirectoryIndex == 0) ? 1 : lastDirectoryIndex); // root or normal folder
	}

	b8 IsRelative(std::string_view filePath)
	{
		return u8path(filePath).is_relative();
	}

	b8 IsDirectory(std::string_view filePath)
	{
		return std::filesystem::is_directory(u8path(filePath));
	}

	std::string TryMakeAbsolute(std::string_view relativePath, std::string_view baseFileOrDirectory)
	{
		if (relativePath.empty() || baseFileOrDirectory.empty() || !IsRelative(relativePath))
			return std::string(relativePath);

		// TODO: Also resolve "../" etc. I guess..?
		std::string baseDirectory { IsDirectory(baseFileOrDirectory) ? baseFileOrDirectory : GetDirectoryName(baseFileOrDirectory) };
		return baseDirectory.append("/").append(relativePath);
	}

	std::string TryMakeRelative(std::string_view absolutePath, std::string_view baseFileOrDirectory)
	{
		std::error_code ec;
		auto basePath = u8path(baseFileOrDirectory);
		if (!std::filesystem::is_directory(basePath, ec))
			basePath = basePath.parent_path();

		auto absoluteFsPath = u8path(absolutePath);
		auto relativePath = std::filesystem::relative(absoluteFsPath, basePath, ec);
		return !CHECK_EC(ec) ? relativePath.u8string() : "";
	}

	std::string TryRemakeRelative(std::string_view path, std::string_view oldBaseFileOrDirectory, std::string_view newBaseFileOrDirectory)
	{
		std::string absPath = Path::TryMakeAbsolute(path, oldBaseFileOrDirectory);
		if (!absPath.empty())
			path = absPath;
		std::string relPath = Path::TryMakeRelative(path, newBaseFileOrDirectory);
		return relPath.empty() ? "" : relPath;
	}

	static std::string& NormalizeInPlace(std::string& inOutFilePath, char DirectorySeparatorReplaced, char DirectorySeparatorNormalized)
	{
		std::replace(inOutFilePath.begin(), inOutFilePath.end(), DirectorySeparatorReplaced, DirectorySeparatorNormalized);
		return inOutFilePath;
	}

	std::string& NormalizeInPlace(std::string& inOutFilePath) { return NormalizeInPlace(inOutFilePath, DirectorySeparatorWin32, DirectorySeparator); }
	std::string& NormalizeInPlaceWin32(std::string& inOutFilePath) { return NormalizeInPlace(inOutFilePath, DirectorySeparator, DirectorySeparatorWin32); }
}

namespace File
{
	UniqueFileContent ReadAllBytes(std::string_view filePath)
	{
		if (filePath.empty())
			return UniqueFileContent {};

		const HANDLE fileHandle = ::CreateFileW(UTF8::WideArg(filePath).c_str(), GENERIC_READ, (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fileHandle == INVALID_HANDLE_VALUE)
			return UniqueFileContent {};

		defer { ::CloseHandle(fileHandle); };

		LARGE_INTEGER largeIntegerFileSize = {};
		if (::GetFileSizeEx(fileHandle, &largeIntegerFileSize) == 0)
			return UniqueFileContent {};

		const size_t fileSize = static_cast<size_t>(largeIntegerFileSize.QuadPart);
		auto fileContent = std::unique_ptr<u8[]>(new u8[fileSize + 1]);

		if (fileContent == nullptr)
			return UniqueFileContent {};

		// HACK: Assume the entire file fits inside a single DWORD for now
		DWORD bytesRead = 0;
		if (::ReadFile(fileHandle, fileContent.get(), static_cast<DWORD>(fileSize), &bytesRead, nullptr) == FALSE)
			return UniqueFileContent {};

		assert(bytesRead == fileSize);
		fileContent[fileSize] = '\0';

		return UniqueFileContent { std::move(fileContent), fileSize };
	}

	b8 WriteAllBytes(std::string_view filePath, const void* fileContent, size_t fileSize)
	{
		if (filePath.empty() || fileContent == nullptr)
			return false;

		const HANDLE fileHandle = ::CreateFileW(UTF8::WideArg(filePath).c_str(), GENERIC_WRITE, (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fileHandle == INVALID_HANDLE_VALUE)
			return false;

		defer { ::CloseHandle(fileHandle); };

		// HACK: Assume the entire file fits inside a single DWORD for now
		DWORD bytesWritten = 0;
		if (::WriteFile(fileHandle, fileContent, static_cast<DWORD>(fileSize), &bytesWritten, nullptr) == FALSE)
			return false;

		return true;
	}

	b8 WriteAllBytes(std::string_view filePath, const UniqueFileContent& uniqueFileContent)
	{
		return WriteAllBytes(filePath, uniqueFileContent.Content.get(), uniqueFileContent.Size);
	}

	b8 WriteAllBytes(std::string_view filePath, const std::string_view textFileContent)
	{
		return WriteAllBytes(filePath, textFileContent.data(), textFileContent.size());
	}

	b8 Exists(std::string_view filePath)
	{
		std::error_code ec;
		auto path = u8path(filePath);
		return std::filesystem::exists(path, ec) && !std::filesystem::is_directory(path, ec);
	}

	b8 Copy(std::string_view source, std::string_view destination, b8 overwriteExisting)
	{
		using copy_options = std::filesystem::copy_options;
		auto options = overwriteExisting ? copy_options::overwrite_existing : copy_options::none;
		std::error_code ec;
		if (!std::filesystem::copy_file(u8path(source), u8path(destination), options, ec)) {
			CHECK_EC(ec);
			return false;
		}
		return true;
	}
}

namespace CommandLine
{
	CommandLineArrayView GetCommandLineUTF8()
	{
		static b8 initialized = false;
		static std::vector<std::string> argvString;
		static std::vector<std::string_view> argvStringViews;

		if (initialized)
			return CommandLineArrayView { argvStringViews.size(), argvStringViews.data() };

		int argc = 0;
		auto argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
		{
			argvString.reserve(argc);
			argvStringViews.reserve(argc);

			for (int i = 0; i < argc; i++)
				argvStringViews.emplace_back(argvString.emplace_back(UTF8::Narrow(argv[i])).c_str());
		}
		::LocalFree(argv);

		initialized = true;
		return CommandLineArrayView { static_cast<size_t>(argc), argvStringViews.data() };
	}
}

namespace Directory
{
	b8 Create(std::string_view directoryPath)
	{
		std::error_code ec;
		if (!std::filesystem::create_directories(u8path(directoryPath), ec)) {
			CHECK_EC(ec);
			return false;
		}
		return true;
	}

	b8 Exists(std::string_view directoryPath)
	{
		return std::filesystem::is_directory(u8path(directoryPath));
	}


	std::string GetExecutablePath()
	{
		// TODO: First ask for size then resize dynamic buffer accordingly
		wchar_t buffer[MAX_PATH] = L"";
		::GetModuleFileNameW(NULL, buffer, MAX_PATH);
		return std::filesystem::path(buffer).u8string();
	}

	std::string GetExecutableDirectory()
	{
		return std::string { Path::GetDirectoryName(GetExecutablePath()) };
	}

	std::string GetWorkingDirectory()
	{
		std::error_code ec;
		auto path = std::filesystem::current_path(ec);
		CHECK_EC(ec);
		return path.u8string();
	}

	b8 SetWorkingDirectory(std::string_view directoryPath)
	{
		std::error_code ec;
		std::filesystem::current_path(u8path(directoryPath), ec);
		return !CHECK_EC(ec);
	}
}

namespace Shell
{
	void OpenInExplorer(std::string_view filePath)
	{
		if (filePath.empty())
			return;

		auto path = u8path(filePath);
		if (Path::IsRelative(filePath))
		{
			std::error_code ec;
			auto absolutePath = std::filesystem::absolute(path, ec);
			if (!CHECK_EC(ec))
				path = absolutePath;
		}
		::ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT);
	}

	MessageBoxResult ShowMessageBox(std::string_view message, std::string_view title, MessageBoxButtons buttons, MessageBoxIcon icon, void* parentWindowHandle)
	{
		UINT flags = 0;
		switch (buttons)
		{
		case MessageBoxButtons::AbortRetryIgnore: flags |= MB_ABORTRETRYIGNORE; break;
		case MessageBoxButtons::CancelTryContinue: flags |= MB_CANCELTRYCONTINUE; break;
		case MessageBoxButtons::OK: flags |= MB_OK; break;
		case MessageBoxButtons::OKCancel: flags |= MB_OKCANCEL; break;
		case MessageBoxButtons::RetryCancel: flags |= MB_RETRYCANCEL; break;
		case MessageBoxButtons::YesNo: flags |= MB_YESNO; break;
		case MessageBoxButtons::YesNoCancel: flags |= MB_YESNOCANCEL; break;
		default: break;
		}
		switch (icon)
		{
		case MessageBoxIcon::Asterisk: flags |= MB_ICONASTERISK; break;
		case MessageBoxIcon::Error: flags |= MB_ICONERROR; break;
		case MessageBoxIcon::Exclamation: flags |= MB_ICONEXCLAMATION; break;
		case MessageBoxIcon::Hand: flags |= MB_ICONHAND; break;
		case MessageBoxIcon::Information: flags |= MB_ICONINFORMATION; break;
		case MessageBoxIcon::None: break;
		case MessageBoxIcon::Question: flags |= MB_ICONQUESTION; break;
		case MessageBoxIcon::Stop: flags |= MB_ICONSTOP; break;
		case MessageBoxIcon::Warning: flags |= MB_ICONWARNING; break;
		default: break;
		}

		const WORD languageID = 0;
		const int result = ::MessageBoxExW(reinterpret_cast<HWND>(parentWindowHandle), UTF8::WideArg(message).c_str(), title.empty() ? nullptr : UTF8::WideArg(title).c_str(), flags, languageID);
		switch (result)
		{
		case IDABORT: return MessageBoxResult::Abort;
		case IDCANCEL: return MessageBoxResult::Cancel;
		case IDCONTINUE: return MessageBoxResult::Continue;
		case IDIGNORE: return MessageBoxResult::Ignore;
		case IDNO: return MessageBoxResult::No;
		case IDOK: return MessageBoxResult::OK;
		case IDRETRY: return MessageBoxResult::Retry;
		case IDTRYAGAIN: return MessageBoxResult::TryAgain;
		case IDYES: return MessageBoxResult::Yes;
		default: return MessageBoxResult::None;
		}
	}
}

namespace Shell
{
	class VeryPogDialogEventHandler : public IFileDialogEvents, public IFileDialogControlEvents
	{
	public:
		VeryPogDialogEventHandler() : refCount(1) {}
		~VeryPogDialogEventHandler() = default;

	public:
#pragma region IUnknown
		IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv)
		{
			static const QITAB searchTable[] =
			{
				QITABENT(VeryPogDialogEventHandler, IFileDialogEvents),
				QITABENT(VeryPogDialogEventHandler, IFileDialogControlEvents),
				{ nullptr, 0 },
			};
			return QISearch(this, searchTable, riid, ppv);
		}

		IFACEMETHODIMP_(ULONG) AddRef()
		{
			return InterlockedIncrement(&refCount);
		}

		IFACEMETHODIMP_(ULONG) Release()
		{
			long cRef = InterlockedDecrement(&refCount);
			if (!cRef)
				delete this;
			return cRef;
		}
#pragma endregion IUnknown

#pragma region IFileDialogEvents
		IFACEMETHODIMP OnFileOk(IFileDialog*) { return S_OK; }
		IFACEMETHODIMP OnFolderChange(IFileDialog*) { return S_OK; }
		IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) { return S_OK; }
		IFACEMETHODIMP OnHelp(IFileDialog*) { return S_OK; }
		IFACEMETHODIMP OnSelectionChange(IFileDialog*) { return S_OK; }
		IFACEMETHODIMP OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE*) { return S_OK; }
		IFACEMETHODIMP OnTypeChange(IFileDialog* pfd) { return S_OK; }
		IFACEMETHODIMP OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*) { return S_OK; }

		IFACEMETHODIMP OnItemSelected(IFileDialogCustomize* pfdc, DWORD dwIDCtl, DWORD dwIDItem)
		{
			HRESULT result = S_OK;
			IFileDialog* fileDialog = nullptr;

			if (result = pfdc->QueryInterface(&fileDialog); !SUCCEEDED(result))
				return result;

#if 0 // NOTE: Add future item check logic here if needed
			switch (dwIDItem)
			{
			default:
				break;
			}
#endif

			fileDialog->Release();
			return result;
		}

		IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize*, DWORD) { return S_OK; }
		IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize*, DWORD, BOOL) { return S_OK; }
		IFACEMETHODIMP OnControlActivating(IFileDialogCustomize*, DWORD) { return S_OK; }
#pragma endregion IFileDialogEvents

	private:
		long refCount;

	public:
		static HRESULT CreateInstance(REFIID riid, void** ppv)
		{
			*ppv = nullptr;
			if (VeryPogDialogEventHandler* pDialogEventHandler = new VeryPogDialogEventHandler(); pDialogEventHandler != nullptr)
			{
				HRESULT result = pDialogEventHandler->QueryInterface(riid, ppv);
				pDialogEventHandler->Release();
				return S_OK;
			}

			return E_OUTOFMEMORY;
		}
	};

	static constexpr uint32_t RandomFileDialogItemBaseID = 0x666;

	static void CreateNativeIFileDialogCustomizeItems(const std::vector<FileDialogItem>& inItems, IFileDialogCustomize* outDialogCustomize)
	{
		HRESULT result = S_OK;
		for (size_t i = 0; i < inItems.size(); i++)
		{
			const auto& inItem = inItems[i];
			const DWORD itemID = static_cast<DWORD>(RandomFileDialogItemBaseID + i);

			if (inItem.Type == FileDialogItemType::VisualGroupStart)
				result = outDialogCustomize->StartVisualGroup(itemID, UTF8::WideArg(inItem.Label).c_str());
			else if (inItem.Type == FileDialogItemType::VisualGroupEnd)
				result = outDialogCustomize->EndVisualGroup();
			else if (inItem.Type == FileDialogItemType::Checkbox)
			{
				result = outDialogCustomize->AddCheckButton(itemID, UTF8::WideArg(inItem.Label).c_str(), (inItem.InOut.CheckboxChecked != nullptr) ? *inItem.InOut.CheckboxChecked : false);

				if (inItem.InOut.CheckboxChecked == nullptr)
					result = outDialogCustomize->SetControlState(itemID, CDCS_VISIBLE);
			}
		}
	}

	static void ReadNativeIFileDialogCustomizeItemState(std::vector<FileDialogItem>& outItems, IFileDialogCustomize* inDialogCustomize)
	{
		HRESULT result = S_OK;
		for (size_t i = 0; i < outItems.size(); i++)
		{
			auto& outItem = outItems[i];
			const DWORD itemID = static_cast<DWORD>(RandomFileDialogItemBaseID + i);

			if (outItem.Type == FileDialogItemType::Checkbox)
			{
				BOOL checked;
				result = inDialogCustomize->GetCheckButtonState(itemID, &checked);
				if (outItem.InOut.CheckboxChecked != nullptr)
					*outItem.InOut.CheckboxChecked = checked;
			}
		}
	}

	enum class DialogType : u8 { Save, Open };
	enum class DialogPickType : u8 { File, Folder };

	static FileDialogResult CreateAndShowFileDialog(FileDialog& dialog, DialogType dialogType, DialogPickType pickType)
	{
		for (const auto& filter : dialog.InFilters)
		{
			assert(!filter.Name.empty() && filter.Name.back() != ')' && "Don't include a file spec suffix");
			assert(!filter.Spec.empty() && filter.Spec[0] == '*' && filter.Spec[1] == '.' && filter.Spec.back() != ';' && "Don't forget the \"*.\" prefix");
		}

		HRESULT hr = S_OK;
		hr = ::CoInitialize(nullptr);
		defer { ::CoUninitialize(); };

		ComPtr<IFileDialog> fileDialog = nullptr;
		hr = ::CoCreateInstance((dialogType == DialogType::Save) ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, __uuidof(fileDialog), &fileDialog);
		if (!SUCCEEDED(hr))
			return FileDialogResult::Error;

		DWORD eventCookie = 0;
		ComPtr<IFileDialogEvents> dialogEvents = nullptr;
		ComPtr<IFileDialogCustomize> dialogCustomize = nullptr;

		if (hr = VeryPogDialogEventHandler::CreateInstance(__uuidof(dialogEvents), &dialogEvents); SUCCEEDED(hr))
		{
			if (hr = fileDialog->Advise(dialogEvents.Get(), &eventCookie); SUCCEEDED(hr))
			{
				if (hr = fileDialog->QueryInterface(__uuidof(dialogCustomize), &dialogCustomize); SUCCEEDED(hr))
					CreateNativeIFileDialogCustomizeItems(dialog.InOutCustomizeItems, dialogCustomize.Get());

				DWORD existingOptionFlags = 0;
				hr = fileDialog->GetOptions(&existingOptionFlags);
				hr = fileDialog->SetOptions(existingOptionFlags | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM | (pickType == DialogPickType::Folder ? FOS_PICKFOLDERS : 0));

				if (!dialog.InTitle.empty())
					hr = fileDialog->SetTitle(UTF8::WideArg(dialog.InTitle).c_str());

				if (!dialog.InFileName.empty())
					hr = fileDialog->SetFileName(UTF8::WideArg(dialog.InFileName).c_str());

				if (!dialog.InDefaultExtension.empty())
					hr = fileDialog->SetDefaultExtension(UTF8::WideArg(ASCII::TrimPrefix(dialog.InDefaultExtension, ".")).c_str());

				if (pickType == DialogPickType::File && !dialog.InFilters.empty())
				{
					// NOTE: Must reserve so the c_str()s won't get invalidated in case of small-string-optimization
					std::vector<std::wstring> owningWideStrings;
					owningWideStrings.reserve(dialog.InFilters.size() * 2);

					std::vector<COMDLG_FILTERSPEC> nonOwningConvertedFilters;
					nonOwningConvertedFilters.reserve(dialog.InFilters.size());

					std::string nameWithSpecSuffix;
					for (const auto& inFilter : dialog.InFilters)
					{
						nameWithSpecSuffix.clear();
						nameWithSpecSuffix += inFilter.Name;
						nameWithSpecSuffix += " (";
						nameWithSpecSuffix += inFilter.Spec;
						nameWithSpecSuffix += ')';

						nonOwningConvertedFilters.push_back(COMDLG_FILTERSPEC
							{
								owningWideStrings.emplace_back(std::move(UTF8::Widen(nameWithSpecSuffix))).c_str(),
								owningWideStrings.emplace_back(std::move(UTF8::Widen(inFilter.Spec))).c_str(),
							});
					}

					hr = fileDialog->SetFileTypes(static_cast<UINT>(nonOwningConvertedFilters.size()), nonOwningConvertedFilters.data());
					hr = fileDialog->SetFileTypeIndex(dialog.InOutFilterIndex);
				}
			}
		}

		if (SUCCEEDED(hr))
		{
			if (hr = fileDialog->Show(reinterpret_cast<HWND>(dialog.InParentWindowHandle)); SUCCEEDED(hr))
			{
				ComPtr<IShellItem> itemResult = nullptr;
				if (hr = fileDialog->GetResult(&itemResult); SUCCEEDED(hr))
				{
					wchar_t* filePath = nullptr;
					if (hr = itemResult->GetDisplayName(SIGDN_FILESYSPATH, &filePath); SUCCEEDED(hr))
					{
						hr = fileDialog->GetFileTypeIndex(&dialog.InOutFilterIndex);
						dialog.OutFilePath = UTF8::Narrow(filePath);

						if (dialogCustomize != nullptr)
							ReadNativeIFileDialogCustomizeItemState(dialog.InOutCustomizeItems, dialogCustomize.Get());

						::CoTaskMemFree(filePath);
					}
				}
			}
		}

		if (fileDialog != nullptr)
			fileDialog->Unadvise(eventCookie);

		if (FAILED(hr))
			return FileDialogResult::Error;

		return !dialog.OutFilePath.empty() ? FileDialogResult::OK : FileDialogResult::Cancel;
	}

	FileDialogResult FileDialog::OpenRead() { return CreateAndShowFileDialog(*this, DialogType::Open, DialogPickType::File); }
	FileDialogResult FileDialog::OpenSave() { return CreateAndShowFileDialog(*this, DialogType::Save, DialogPickType::File); }
	FileDialogResult FileDialog::OpenSelectFolder() { return CreateAndShowFileDialog(*this, DialogType::Open, DialogPickType::Folder); }
}

#include "chart_editor_i18n.h"

namespace PeepoDrumKit::i18n
{
	static std::map<u32, std::string> HashStringMap;
	static std::shared_mutex HashStringMapMutex;
	std::string SelectedFontName = "NotoSansCJKjp-Regular.otf";
	std::vector<LocaleEntry> LocaleEntries;

	static void InitBuiltinLocaleWithoutLock()
	{
		HashStringMap.clear();

#define X(en, jp) HashStringMap[Hash(en)] = std::string(en);
		PEEPODRUMKIT_UI_STRINGS_X_MACRO_LIST_JA
#undef X
	}

	void InitBuiltinLocale()
	{
		HashStringMapMutex.lock();
		InitBuiltinLocaleWithoutLock();
		HashStringMapMutex.unlock();
	}

	cstr HashToString(u32 inHash)
	{
		HashStringMapMutex.lock_shared();
		// if (HashStringMap.empty()) InitBuiltinLocale();
		for (auto it = HashStringMap.begin(); it != HashStringMap.end(); ++it)
			if (it->first == inHash)
			{
				cstr result = it->second.c_str();
				HashStringMapMutex.unlock_shared();
				return result;
			}
		HashStringMapMutex.unlock_shared();

#if PEEPO_DEBUG
		assert(!"Missing string entry"); return nullptr;
#endif
		return "(undefined)";
	}

	void ExportBuiltinLocaleFiles()
	{
		std::filesystem::create_directories("locales");
		{
			std::fstream localeFile("locales/en.txt", std::ios::out | std::ios::trunc);

			localeFile << "Name: English" << std::endl;
			localeFile << "Lang: en" << std::endl;
			localeFile << "Font: NotoSansCJKjp-Regular.otf" << std::endl << std::endl;

#define X(en, ja) \
			(localeFile << std::hex << std::setw(8) << std::setfill('0') << Hash(en)); \
			(localeFile << ": " << en << std::endl);
			PEEPODRUMKIT_UI_STRINGS_X_MACRO_LIST_JA
#undef X
		}

		{
			std::fstream localeFile("locales/jp.txt", std::ios::out | std::ios::trunc);

			localeFile << u8"Name: 日本語" << std::endl;
			localeFile << "Lang: jp" << std::endl;
			localeFile << "Font: NotoSansCJKjp-Regular.otf" << std::endl << std::endl;

#define X(en, ja) \
			(localeFile << std::hex << std::setw(8) << std::setfill('0') << Hash(en)); \
			(localeFile << ": " << ja << std::endl);
			PEEPODRUMKIT_UI_STRINGS_X_MACRO_LIST_JA
#undef X
		}

		{
			std::fstream localeFile("locales/zh-cn.txt", std::ios::out | std::ios::trunc);

			localeFile << u8"Name: 中文（中国）" << std::endl;
			localeFile << "Lang: zh-cn" << std::endl;
			localeFile << "Font: NotoSansCJKjp-Regular.otf" << std::endl << std::endl;

#define X(en, ja) \
			(localeFile << std::hex << std::setw(8) << std::setfill('0') << Hash(en)); \
			(localeFile << ": " << ja << std::endl);
			PEEPODRUMKIT_UI_STRINGS_X_MACRO_LIST_ZHCN
#undef X
		}

		{
			std::fstream localeFile("locales/zh-tw.txt", std::ios::out | std::ios::trunc);

			localeFile << u8"Name: 中文（台灣）" << std::endl;
			localeFile << "Lang: zh-tw" << std::endl;
			localeFile << "Font: NotoSansCJKjp-Regular.otf" << std::endl << std::endl;

#define X(en, ja) \
			(localeFile << std::hex << std::setw(8) << std::setfill('0') << Hash(en)); \
			(localeFile << ": " << ja << std::endl);
			PEEPODRUMKIT_UI_STRINGS_X_MACRO_LIST_ZHTW
#undef X
		}
	}

	void RefreshLocales()
	{
		HashStringMapMutex.lock();
		LocaleEntries.clear();
        LocaleEntries.push_back(LocaleEntry {
			std::string("en"),
			std::string("English")
		});

		std::filesystem::directory_iterator dirIter("locales");
		for (const auto& entry : dirIter)
		{
			if (entry.is_regular_file())
			{
				std::fstream localeFile(entry.path(), std::ios::in);
				if (!localeFile.is_open()) continue;
				std::string line;
				LocaleEntry localeEntry;
				bool nameFound = false;
				bool langFound = false;
				while (std::getline(localeFile, line))
				{
					if (line.empty()) continue;
					if (line[0] == '#') continue;
					if (line.find("Name: ") != std::string::npos && !nameFound)
					{
						localeEntry.name = line.substr(6);
						nameFound = true;
					}
					else if (line.find("Lang: ") != std::string::npos && !langFound)
					{
						localeEntry.id = line.substr(6);
						langFound = true;
					}
					if (nameFound && langFound) break;
				}
				localeFile.close();
				if (nameFound && langFound && localeEntry.id != "en")
					LocaleEntries.push_back(localeEntry);
			}
		}
		HashStringMapMutex.unlock();
	}

	void ReloadLocaleFile(cstr languageId)
	{
		HashStringMapMutex.lock();
		InitBuiltinLocaleWithoutLock();
		std::string localeFilePath = "locales/" + std::string(languageId) + ".txt";
		std::fstream localeFile(localeFilePath, std::ios::in);
		if (!localeFile.is_open())
		{
			localeFile.close();
			localeFile.open(localeFilePath, std::ios::in);
		}
		std::string line;
		while (std::getline(localeFile, line))
		{
			if (line.empty()) continue;
			if (line[0] == '#') continue;
			size_t colonPos = line.find(':');
			if (colonPos == std::string::npos) continue;
			std::string hashStr = line.substr(0, colonPos);
			if (hashStr.length() == 8)
			{
				try {
					u32 hash = std::stoul(hashStr, nullptr, 16);
					HashStringMap[hash] = line.substr(colonPos + 2);
					std::cout << "Override hash "  << hashStr << " to " << HashStringMap[hash] << std::endl;
				}
				catch (...)
				{
					continue;
				}
			}
		}
		HashStringMapMutex.unlock();
	}
}

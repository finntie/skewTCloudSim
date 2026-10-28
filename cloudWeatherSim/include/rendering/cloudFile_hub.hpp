#pragma once

#include <string>


struct renderSettings;
struct cloudFileData;
class cloudFile;

class cloudFileHub
{
public:

	cloudFileHub(cloudFile* cloudFileObject);
	~cloudFileHub();

	void saveCloudFilePanel();
	void saveRenderSettingsPanel(renderSettings& settings);
	// Main cloud view menu, returns true if need to update the renderer
	bool cloudViewMenuPanel();

private:


	// ImGui Helpers
	void deletePopup(cloudFileData& CFData);
	void confirmPopup(cloudFileData& CFData);


	bool overWriteFilePopup(const char* file);

	bool checkFile(const char* fileName, std::string& outputFullPath);

	cloudFile* m_cloudFileObj = nullptr;
};


// Helper function
bool coloredSelectable(const char* label, bool* value);
#include <boost/property_tree/ptree.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/optional.hpp>

#include <regex>
#include <vector>

using namespace boost::property_tree;
namespace bfs = boost::filesystem;

class SProjectInfo
{
public:

	class	SSrcDir
	{
	public:
		bfs::path	From_;
		bfs::path	To_;
		bool		AddToIncludeDir_ = false;
	};

	typedef	std::vector<std::string>	Vector;
	typedef	std::vector<SSrcDir>		SrcDirList;

	std::string	TargetName;
	bfs::path	ProjectBuildPath;
	SSrcDir		VCXProjectPath;
	SrcDirList	SrcList;
	Vector		AdditionalIncludeDirectories;
	Vector		AdditionalDependencies;
	Vector		AdditionalLibraryDirectories;
};

boost::optional<SProjectInfo>	ReadConfig();
bool			BuildProject(const SProjectInfo& projInfo);
#include "ProjConvertor.h"

#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range.hpp>

#include <iostream>
#include <map>
#include <functional>

class	PathConverter
{
public:

	PathConverter()
	{
		{//TargetName
			auto ftr = [](const SProjectInfo& projInfo)
			{
				return projInfo.TargetName;
			};

			ConverterMap_.emplace("$(TargetName)", ftr);
		}

		{//ProjectBuildPath
			auto ftr = [](const SProjectInfo& projInfo)
			{
				return projInfo.ProjectBuildPath.string() + "\\";
			};

			ConverterMap_.emplace("$(ProjectBuildPath)", ftr);
		}

		{//VCXProjectPath
			auto ftr = [](const SProjectInfo& projInfo)
			{
				return projInfo.VCXProjectPath.To_.string() + "\\";
			};

			ConverterMap_.emplace("$(VCXProjectPath)", ftr);
		}
	}

	static	const PathConverter&	GetInstance()
	{
		static	PathConverter sIns;
		return sIns;
	}

	std::string	ConvertPath(const std::string& to, const SProjectInfo& projInfo) const
	{
		auto ret = to;
		for ( auto& curConvert : ConverterMap_ )
		{
			auto toFind = curConvert.first;
			auto repleace = curConvert.second(projInfo);
			boost::algorithm::replace_all(ret, toFind, repleace);
		}

		return ret;
	}

private:

	typedef	std::function<std::string(const SProjectInfo&)>	Converter;
	std::map<std::string, Converter>	ConverterMap_;
};

bfs::path RelativeTo(const bfs::path& from, const bfs::path& to)
{
	// Start at the root path and while they are the same then do nothing then when they first
	// diverge take the remainder of the two path and replace the entire from path with ".."
	// segments.
	auto cpyFrom = from;
	auto cpyTo = to;
	cpyFrom.remove_trailing_separator();
	cpyTo.remove_trailing_separator();

	auto fromIter = cpyFrom.begin();
	auto toIter = cpyTo.begin();

	// Loop through both
	while ( fromIter != cpyFrom.end() && toIter != cpyTo.end() && (*toIter) == (*fromIter) )
	{
		++toIter;
		++fromIter;
	}

	bfs::path finalPath;
	while ( fromIter != cpyFrom.end() )
	{
		if ( !(finalPath.empty() && *fromIter == ".") )
		{
			finalPath /= "..";
		}

		++fromIter;
	}
	
	while ( toIter != cpyTo.end() )
	{
		if ( *toIter != "." )
		{
			finalPath /= *toIter;
		}

		++toIter;
	}

	return finalPath;
}

boost::optional<SProjectInfo> ReadConfig()
{
	SProjectInfo projInfo;

	if ( !bfs::exists("config.xml") )
	{
		std::cerr << "Need Config File." << std::endl;
		return boost::none;
	}

	boost::filesystem::ifstream configIfs("config.xml");
	boost::property_tree::ptree configXml;

	try
	{
		boost::property_tree::read_xml(configIfs, configXml, xml_parser::no_comments | xml_parser::trim_whitespace);
	}
	catch ( std::exception& exp )
	{
		std::cerr << exp.what() << std::endl;
		return boost::none;
	}

	{//TargetName
		auto targetName = configXml.get_optional<std::string>("Project.TargetName.<xmlattr>.Name");
		if ( !targetName )
		{
			std::cerr << "Need Target Name." << std::endl;
			return boost::none;
		}
		projInfo.TargetName = *targetName;
	}

	{//ProjectBuildPath
		auto projPath = configXml.get_optional<std::string>("Project.ProjectBuildPath.<xmlattr>.Path");
		if ( !projPath )
		{
			std::cerr << "Need Project Path." << std::endl;
			return boost::none;
		}
		projInfo.ProjectBuildPath = bfs::system_complete(*projPath);
		projInfo.ProjectBuildPath.remove_trailing_separator();
	}

	{//VCXProjectPath
		auto vcxFromPath = configXml.get_optional<std::string>("Project.VCXProjectPath.<xmlattr>.From");
		auto vcxToPath = configXml.get_optional<std::string>("Project.VCXProjectPath.<xmlattr>.To");
		if ( !vcxFromPath )
		{
			std::cerr << "Need Project Build Path." << std::endl;
			return boost::none;
		}
		projInfo.VCXProjectPath.From_ = bfs::system_complete(*vcxFromPath);
		projInfo.VCXProjectPath.To_ = bfs::system_complete(PathConverter::GetInstance().ConvertPath(*vcxToPath, projInfo));

		projInfo.VCXProjectPath.From_.remove_trailing_separator();
		projInfo.VCXProjectPath.To_.remove_trailing_separator();
	}

	{//SrcDirectories
		auto srcDirectories = configXml.get_child_optional("Project.SrcDirectories");
		if ( srcDirectories )
		{
			for ( auto& curAdditionalDep : *srcDirectories )
			{
				auto from = curAdditionalDep.second.get_optional<std::string>("<xmlattr>.From");
				auto to = curAdditionalDep.second.get_optional<std::string>("<xmlattr>.To");
				auto addToIncDir = curAdditionalDep.second.get_optional<std::string>("<xmlattr>.AddToIncludeDir");

				SProjectInfo::SSrcDir newDir;
				newDir.From_ = bfs::system_complete(*from);
				newDir.To_ = bfs::system_complete(PathConverter::GetInstance().ConvertPath(*to, projInfo));
				newDir.AddToIncludeDir_ = *addToIncDir == "True";

				newDir.From_.remove_trailing_separator();
				newDir.To_.remove_trailing_separator();
				projInfo.SrcList.push_back(newDir);

				if ( newDir.AddToIncludeDir_ )
				{
					projInfo.AdditionalIncludeDirectories.push_back(RelativeTo(projInfo.VCXProjectPath.To_, newDir.To_).string());
				}
			}
		}
	}

	auto additionalDepends = configXml.get_child_optional("Project.AdditionalDependencies");
	if ( additionalDepends )
	{
		for ( auto& curAdditionalDep : *additionalDepends )
		{
			auto item = curAdditionalDep.second.get_optional<std::string>("<xmlattr>.Path");
			if ( item )
			{
				projInfo.AdditionalDependencies.push_back(*item);
			}
		}
	}

	auto additionalLibsDirs = configXml.get_child_optional("Project.AdditionalLibraryDirectories");
	if ( additionalLibsDirs )
	{
		for ( auto& curAdditionalLibDir : *additionalLibsDirs )
		{
			auto item = curAdditionalLibDir.second.get_optional<std::string>("<xmlattr>.Path");
			if ( item )
			{
				projInfo.AdditionalLibraryDirectories.push_back(*item);
			}
		}
	}

	return projInfo;
}

bool CheckConfig(const SProjectInfo& projInfo)
{
	auto projPath = projInfo.VCXProjectPath.From_ / (projInfo.TargetName + ".vcxproj");
	if ( !bfs::exists(projPath) )
	{
		std::cerr << "Can not Find " << bfs::absolute(projPath) << std::endl;
		return false;
	}

	auto filterPath = projInfo.VCXProjectPath.From_ / (projInfo.TargetName + ".vcxproj.filters");
	if ( !bfs::exists(filterPath) )
	{
		std::cerr << "Can not Find " << bfs::absolute(filterPath) << std::endl;
		return false;
	}

	return true;
}

bool	BuildVCXPROJ(const SProjectInfo& projInfo)
{
	auto projFileName = projInfo.VCXProjectPath.From_ / (projInfo.TargetName + ".vcxproj");

	ptree rawProjXml, outputProjXml;

	try
	{
		boost::filesystem::fstream projIfs(projFileName);
		boost::property_tree::read_xml(projIfs, rawProjXml, xml_parser::no_comments | xml_parser::trim_whitespace);
	}
	catch ( std::exception& exp )
	{
		std::cerr << exp.what() << std::endl;
		return false;
	}

	auto projItems = rawProjXml.get_child_optional("Project");
	if ( !projItems )
	{
		std::cerr << "Can not find <Project>." << std::endl;
		return false;
	}

	for ( auto& curSrcDir : projInfo.SrcList )
	{
		if ( bfs::exists(curSrcDir.To_) )
		{
			bfs::remove_all(curSrcDir.To_);
		}

		bfs::create_directories(curSrcDir.To_);
	}

	for ( auto& curProjItem : *projItems )
	{
		if ( curProjItem.first == "PropertyGroup" )
		{
			auto outDir = curProjItem.second.get_optional<std::string>("OutDir");
			if ( !outDir )
			{
				outputProjXml.add_child("Project." + curProjItem.first, curProjItem.second);
				continue;
			}

			ptree tmpPG;
			for ( auto& curPropertyGroupItem : curProjItem.second )
			{
				if ( curPropertyGroupItem.first == "OutDir" )
				{
					auto dir = curPropertyGroupItem.second.get_value<std::string>();
					ptree outDir;
					outDir.add_child("<xmlattr>", curPropertyGroupItem.second.get_child("<xmlattr>"));
					outDir.put_value(R"($(SolutionDir)build\bin\$(Configuration)\$(Platform)\$(PlatformToolset)\)");

					tmpPG.add_child(curPropertyGroupItem.first, outDir);
				}
				else if ( curPropertyGroupItem.first == "IntDir" )
				{
					auto dir = curPropertyGroupItem.second.get_value<std::string>();
					ptree intDir;
					intDir.add_child("<xmlattr>", curPropertyGroupItem.second.get_child("<xmlattr>"));
					intDir.put_value(R"($(SolutionDir)build\obj\$(ProjectName)\$(Configuration)\$(Platform)\$(PlatformToolset)\)");

					tmpPG.add_child(curPropertyGroupItem.first, intDir);
				}
				else if ( curPropertyGroupItem.first != "TargetName" )
				{
					tmpPG.add_child(curPropertyGroupItem.first, curPropertyGroupItem.second);
				}
			}
			outputProjXml.add_child("Project.PropertyGroup", tmpPG);
		}
		else if ( curProjItem.first == "ItemDefinitionGroup" )
		{
			ptree tmpIDG;
			//tmpIDG.add_child("<xmlattr>", curProjItem.second.get_child("<xmlattr>"));

			static std::regex rg(R"((.+?)(;|$))");

			for ( auto& curIDGItem : curProjItem.second )
			{
				//curIDGItem.first will be "ClCompile"/"ResourceCompile"/"Midl"/"Link"/"ProjectReference"

				for ( auto& curID : curIDGItem.second )
				{
					if ( curID.first == "AdditionalIncludeDirectories" )
					{
						std::string finalStr;
						for ( auto& curStr : projInfo.AdditionalIncludeDirectories )
						{
							finalStr += curStr + ";";
						}
						finalStr += "%(AdditionalIncludeDirectories)";

						tmpIDG.add(curIDGItem.first + "." + curID.first, finalStr);
					}
					else if ( curID.first == "PreprocessorDefinitions" )
					{
						auto rawStr = curID.second.get_value<std::string>();

						std::sregex_iterator itor(rawStr.begin(), rawStr.end(), rg), itorEnd;

						std::string finalStr;
						for ( auto& curStr : boost::make_iterator_range(itor, itorEnd) )
						{
							auto predef = curStr.str(1);
							if ( predef != "BOOST_ALL_NO_LIB" )
							{
								finalStr += curStr.str(1) + ";";
							}
						}

						finalStr.erase(finalStr.size() - 1);

						tmpIDG.add(curIDGItem.first + "." + curID.first, finalStr);
					}
					else if ( curID.first == "AdditionalDependencies" )
					{
						std::string finalStr;
						for ( auto& curStr : projInfo.AdditionalDependencies )
						{
							finalStr += curStr + ";";
						}
						finalStr += "%(AdditionalDependencies)";

						tmpIDG.add(curIDGItem.first + "." + curID.first, finalStr);
					}
					else if ( curID.first == "AdditionalLibraryDirectories" )
					{
						std::string finalStr;
						for ( auto& curStr : projInfo.AdditionalLibraryDirectories )
						{
							finalStr += curStr + ";";
						}
						finalStr += "%(AdditionalLibraryDirectories)";

						tmpIDG.add(curIDGItem.first + "." + curID.first, finalStr);
					}
					else if ( curID.first == "ImportLibrary" )
					{
						tmpIDG.add(curIDGItem.first + "." + curID.first, R"($(SolutionDir)build\lib\$(ProjectName)\$(Configuration)\$(Platform)\$(PlatformToolset)\$(TargetName).lib)");
					}
					else if ( curID.first != "AssemblerListingLocation" && curID.first != "ProgramDataBaseFile" )
					{
						tmpIDG.add_child(curIDGItem.first + "." + curID.first, curID.second);
					}
				}

				if ( curIDGItem.first == "ClCompile" )
				{
					tmpIDG.put(curIDGItem.first + ".ProgramDataBaseFileName", R"($(IntDir)vc$(PlatformToolsetVersion)$(TargetName).pdb)");
				}
			}

			outputProjXml.add_child("Project.ItemDefinitionGroup", tmpIDG);
		}
		else if ( curProjItem.first == "ItemGroup" )
		{
			ptree tmpIG;

			for ( auto& curItemGroup : curProjItem.second )
			{
				if ( curItemGroup.first == "ClInclude" )//TODO:H文件
				{
					ptree tmpFile;

					auto hFile = curItemGroup.second.get<std::string>("<xmlattr>.Include");

					bfs::path filePath = hFile;
					if ( !filePath.is_absolute() )
					{
						filePath = bfs::system_complete(projInfo.VCXProjectPath.From_ / hFile);
					}

					auto found = false;
					for ( auto& curDir : projInfo.SrcList )
					{
						auto curRelPath = RelativeTo(curDir.From_, filePath);
						if ( *curRelPath.begin() == ".." )
						{
							continue;
						}

						auto cpyPath = curDir.To_ / curRelPath;
						bfs::copy_file(filePath, cpyPath, bfs::copy_option::overwrite_if_exists);

						auto relFromProj = RelativeTo(projInfo.VCXProjectPath.To_, cpyPath);
						tmpFile.add("<xmlattr>.Include", relFromProj.string());

						found = true;
						break;
					}
					
					if ( !found )
					{
						auto cpyPath = projInfo.VCXProjectPath.To_ / "../Other/";
						bfs::create_directories(cpyPath);

						auto writePath = "../Other/" + filePath.filename().string();
						tmpFile.add("<xmlattr>.Include", writePath);
						bfs::copy_file(filePath, cpyPath / filePath.filename(), bfs::copy_option::overwrite_if_exists);
					}

					tmpIG.add_child(curItemGroup.first, tmpFile);
				}
				else if ( curItemGroup.first == "ClCompile" || curItemGroup.first == "ResourceCompile" )//TODO:CPP文件
				{
					ptree tmpFile;

					for ( auto& cppItem : curItemGroup.second )
					{
						if ( cppItem.first == "<xmlattr>" )
						{
							auto cppFile = curItemGroup.second.get<std::string>("<xmlattr>.Include");
							
							bfs::path filePath = cppFile;
							if ( !filePath.is_absolute() )
							{
								filePath = bfs::system_complete(projInfo.VCXProjectPath.From_ / cppFile);
							}

							auto found = false;
							for ( auto& curDir : projInfo.SrcList )
							{
								auto curRelPath = RelativeTo(curDir.From_, filePath);
								if ( *curRelPath.begin() == ".." )
								{
									continue;
								}

								auto cpyPath = curDir.To_ / curRelPath;
								bfs::copy_file(filePath, cpyPath, bfs::copy_option::overwrite_if_exists);

								auto relFromProj = RelativeTo(projInfo.VCXProjectPath.To_, cpyPath);
								tmpFile.add("<xmlattr>.Include", relFromProj.string());

								found = true;
								break;
							}

							if ( !found )
							{
								auto cpyPath = projInfo.VCXProjectPath.To_ / "../Other/";
								bfs::create_directories(cpyPath);

								auto writePath = "../Other/" + filePath.filename().string();
								tmpFile.add("<xmlattr>.Include", writePath);
								bfs::copy_file(filePath, cpyPath / filePath.filename(), bfs::copy_option::overwrite_if_exists);
							}
						}
						else
						{
							tmpFile.add_child(cppItem.first, cppItem.second);
						}
					}

					tmpIG.add_child(curItemGroup.first, tmpFile);
				}
				else
				{
					tmpIG.add_child(curItemGroup.first, curItemGroup.second);
				}
			}

			outputProjXml.add_child("Project." + curProjItem.first, tmpIG);
		}
		else
		{
			outputProjXml.add_child("Project." + curProjItem.first, curProjItem.second);
		}

	}
	
	{
		bfs::create_directories(projInfo.VCXProjectPath.To_);

		boost::filesystem::fstream ofs(projInfo.VCXProjectPath.To_ / (projInfo.TargetName + ".vcxproj"), std::ios::trunc | std::ios::out);
		auto settings = xml_writer_settings<std::string>();
		settings.indent_count = 2;
		//settings.indent_char = '\t';
		write_xml(ofs, outputProjXml, settings);
	}

	return true;
}

bool	BuildFilter(const SProjectInfo& projInfo)
{
	auto filterFileName = projInfo.VCXProjectPath.From_ / (projInfo.TargetName + ".vcxproj.filters");

	ptree rawFilterXml, outputFilterXml;

	try
	{
		boost::filesystem::fstream filterIfs(filterFileName);
		boost::property_tree::read_xml(filterIfs, rawFilterXml, xml_parser::no_comments | xml_parser::trim_whitespace);
	}
	catch ( std::exception& exp )
	{
		std::cerr << exp.what() << std::endl;
		return false;
	}

	auto projItems = rawFilterXml.get_child_optional("Project");
	if ( !projItems )
	{
		std::cerr << "Can not find <Project>." << std::endl;
		return false;
	}

	for ( auto& curProjItem : *projItems )
	{
		if ( curProjItem.first == "ItemGroup" )
		{
			ptree tmpIG;

			for ( auto& curItem : curProjItem.second )
			{
				if ( curItem.first == "Filter" )
				{
					tmpIG.add_child(curItem.first, curItem.second);
				}
				else
				{
					ptree item; //Compile

					for ( auto& curItemProperty : curItem.second )
					{
						if ( curItemProperty.first == "<xmlattr>" )
						{
							bfs::path file = curItemProperty.second.get<std::string>("Include");

							auto filePath = file.is_absolute() ? file : bfs::system_complete(projInfo.VCXProjectPath.From_ / file);
							auto parentPath = filePath.parent_path();
							
							auto found = false;
							for ( auto& curDir : projInfo.SrcList )
							{
								if ( curDir.From_ != parentPath )
								{
									continue;
								}

								auto relPath = RelativeTo(projInfo.VCXProjectPath.To_, curDir.To_);
								item.add("<xmlattr>.Include", (relPath / filePath.filename()).string());

								found = true;
								break;
							}

							assert(found);
						}
						else
						{
							item.add_child(curItemProperty.first, curItemProperty.second);
						}
					}

					tmpIG.add_child(curItem.first, item);
				}
			}

			outputFilterXml.add_child("Project." + curProjItem.first, tmpIG);
		}
		else
		{
			outputFilterXml.add_child("Project." + curProjItem.first, curProjItem.second);
		}
	}

	{
		bfs::create_directories(projInfo.VCXProjectPath.To_);

		boost::filesystem::fstream ofs(projInfo.VCXProjectPath.To_ / (projInfo.TargetName + ".vcxproj.filters"), std::ios::trunc | std::ios::out);
		auto settings = xml_writer_settings<std::string>();
		settings.indent_count = 2;

		write_xml(ofs, outputFilterXml, settings);
	}

	return true;
}

bool BuildProject(const SProjectInfo& projInfo)
{
	if ( !CheckConfig(projInfo) )
	{
		return false;
	}

 	if ( !BuildVCXPROJ(projInfo) )
 	{
 		return false;
 	}

	if ( !BuildFilter(projInfo) )
	{
		return false;
	}

	return true;
}

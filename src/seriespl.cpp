/*
 *	Copyright © 2016 tastytea <tastytea@tastytea.de>
 *
 *	This file is part of seriespl.
 *
 *	seriespl is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2 of the License.
 *
 *	seriespl is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with seriespl. If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include <memory>
#include <iostream>
#include <string>
#include <regex>
#include <iterator>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <getopt.h>
#include <map>
#include <utility>
#include <cstdio>

#include "seriespl.hpp"
#include "http.hpp"
#include "config.hpp"

Seriespl::Seriespl()
{
	Providers =
	{ // Set default list of active streaming providers, SSL only
		Streamcloud,
		Vivo,
		Shared,
		YouTube,
		OpenLoad
	};
}

Seriespl::~Seriespl() {}

int Seriespl::start()
{
	if (directoryurl.find("bs.to") != std::string::npos)
	{
		service = BurningSeries;
	}
	else
	{
		std::cerr << "Error: Could not determine which website you specified, " <<
			"defaulting to Burning-Series." << std::endl;
		sleep(2);
		service = BurningSeries;
	}

	// read config and set streaming providers, if specified
	if (readconfig(config))
	{
		if (config["streamproviders"] != "")
			populate_providers(config["streamproviders"]);
		if (config["youtube-dl"] != "")
			yt_dl_path = config["youtube-dl"];
	}

	if (service == BurningSeries)
	{
		std::string provider_re = "(";
		std::vector<StreamProviders>::iterator it;

		for (it = Providers.begin(); it != Providers.end(); ++it)
		{ // Build regular expression for all supported streaming providers
			if (it != Providers.begin())
			{ // Add | unless it is the first match
				provider_re += "|";
			}
			provider_re += providermap.at(*it).first;
		}
		provider_re += ")";

		for (short season = startSeason; season <= endSeason; ++season)
		{
			if (season != -1)
			{ // A season range was selected
				//FIXME: If season is higher than available seasons, season 1 is returned
				std::regex reSeries("(https://bs.to/serie/[^/]*/).*");
				std::smatch match;
			
				// Generate URL for each season
				if (std::regex_search(directoryurl, match, reSeries))
				{
					directoryurl = match[1].str() + std::to_string(season);
				}
				else
				{
					directoryurl = directoryurl + "/" + std::to_string(season);
				}
				content = getpage(directoryurl);
			}
			else
			{ // If no season range was selected, use supplied URL
				content = getpage(directoryurl);
			}

			if (current_episode != 0)
			{
				std::regex reEpisode("https://bs.to/serie/[^/]*/[[:digit:]]+/([[:digit:]]+)-.*");
				std::smatch match;

				if (std::regex_search(directoryurl, match, reEpisode))
				{
					if ((current_episode & 1) != 0)
					{
						startEpisode = std::stoi(match[1].str());
					}
					if ((current_episode & 2) != 0)
					{
						endEpisode = std::stoi(match[1].str());
					}
				}
				else
				{
					std::cerr << "Error: Could not extract current episode." << std::endl;
					return 3;
				}
			}

			std::regex reEpisodePage("href=\"(serie/.*/[[:digit:]]+/([[:digit:]]+)-.*/(" +
				provider_re + ")-[0-9])\">(" + provider_re + ")?</a>");
			std::sregex_iterator it_re(content.begin(), content.end(), reEpisodePage);
			std::sregex_iterator it_re_end;

			if (it_re == it_re_end)
			{ // No matches
				std::cerr << "Error: No episodes found" << std::endl;
				return 4;
			}
			while (it_re != it_re_end)
			{ // 1 == link, 2 == episode, 3 == provider
				static short episode = 0;
				if (episode == 0 && startEpisode > 0)
				{ // Set episode to the right starting value if episode range is specified
					episode = startEpisode - 1;
				}
				if (std::stoi((*it_re)[2]) >= startEpisode &&
					std::stoi((*it_re)[2]) <= endEpisode &&
					std::stoi((*it_re)[2]) != episode)
				{
					if ( stoi((*it_re)[2]) > (episode + 1) )
					{ // If current episode is > last episode + 1, we are missing an episode
						std::cerr << "Error: Could not get URL for episode " << (episode + 1);
						if ((episode + 2) != stoi((*it_re)[2]))
						{ // If more than 1 episode is missing
							std::cerr << "-" << (stoi((*it_re)[2]) - 1);
						}
						std::cerr << "." << std::endl;
					}
					std::string episodelink = "https://bs.to/" + (*it_re)[1].str();
					std::map<StreamProviders, providerpair>::const_iterator it;
					for (it = providermap.begin(); it != providermap.end(); ++it)
					{
						if (it->second.first == (*it_re)[3])
						{
							std::string streamtitle;
							std::string streamurl = getlink(episodelink, it->first, streamtitle);
							if (direct_url)
							{
								print_playlist(playlist, get_direct_url(streamurl), streamtitle);
							}
							else
							{
								print_playlist(playlist, streamurl, streamtitle);
							}

						}
					}
					episode = std::stoi((*it_re)[2]);
				}
				++it_re;
			} // Iterating through matches
			if (playlist == PL_PLS)
			{ // Print NumberOfEntries
				print_playlist(PL_PLS, "NUMBER_OF_EPISODES");
			}
		}
	}

	return 0;
}

int Seriespl::handle_args(int argc, char const *argv[])
{
	int opt;
	std::string usage = std::string("usage: ") + argv[0] +
		" [-h] [-i]|[-p list] [-e episodes] [-s seasons] [-f format] [-y] URL";
	
	while ((opt = getopt(argc, (char **)argv, "hp:ie:s:f:yV")) != -1)
	{
		std::string episodes, seasons;
		size_t pos;
		switch (opt)
		{
			case 'h':	// Help
				std::cout << usage << std::endl << std::endl;
				std::cout <<
					"  -h                   Show this help" << std::endl;
				std::cout <<
					"  -p stream providers  Comma delimited list. Available:" << std::endl;
				std::cout <<
					"                       Streamcloud,Vivo,Shared,YouTube,OpenLoad,PowerWatch,CloudTime,AuroraVid,Vidto" << std::endl;
				std::cout <<
					"  -i                   Use stream providers without SSL support too" << std::endl;
				std::cout <<
					"  -e                   Episode range, e.g. 2-5 or 7 or 9-, use c for current" << std::endl;
				std::cout <<
					"  -s                   Season range, e.g. 1-2 or 4" << std::endl;
				std::cout <<
					"  -f                   Playlist format. Available: raw, m3u, pls" << std::endl;
				std::cout <<
					"  -y                   Use youtube-dl to print the direct URL of the video file" << std::endl;
				std::cout <<
					"  -V                   Output version and copyright information and exit" << std::endl;
				return 0;
				break;
			case 'p':	// Provider
				populate_providers(std::string(optarg));
				break;
			case 'i':	// Insecure
				Providers =
				{
					Streamcloud,
					Vivo,
					Shared,
					YouTube,
					OpenLoad,
					PowerWatch,
					CloudTime,
					AuroraVid,
					Vidto
				};
				break;
			case 'e':	// Episodes
				episodes = optarg;
				pos = episodes.find('-');
				if (pos != std::string::npos)
				{
					try
					{
						if (episodes.substr(0, pos) == "c")
						{
							current_episode = 1;
						}
						else
						{
							startEpisode = std::stoi( episodes.substr(0, pos) );
						}
						if (episodes.length() > pos + 1)
						{ // If episodes = 5-, output all episodes, beginning with 5
							if (episodes.substr(pos + 1) == "c")
							{
								if (current_episode == 1)
								{
									current_episode = 3;
								}
								else
								{
									current_episode = 2;
								}
							}
							else
							{
								endEpisode = std::stoi( episodes.substr(pos + 1) );
							}
						}
					}
					catch (std::exception &e)
					{ // There is a '-' but no numbers around it
						std::cerr << "Error: Can not decipher episode range, " <<
							"defaulting to all." << std::endl;
						sleep(2);
					}
				}
				else
				{
					try
					{ 
						if (episodes == "c")
						{
							current_episode = 3;
						}
						else
						{ // Is episodes a single number?
							startEpisode = std::stoi(episodes);
							endEpisode = std::stoi(episodes);
						}
					}
					catch (std::exception &e)
					{
						std::cerr << "Error: Can not decipher episode range, " <<
							"defaulting to all." << std::endl;
						sleep(2);
					}
				}
				break;
			case 's':	// Seasons
				seasons = optarg;
				pos = seasons.find('-');
				if (pos != std::string::npos)
				{
					try
					{
						startSeason = std::stoi( seasons.substr(0, pos) );
						endSeason = std::stoi( seasons.substr(pos + 1) );
					}
					catch (std::exception &e)
					{ // There is a '-' but no numbers around it
						std::cerr << "Error: Can not decipher season range, " <<
							"defaulting to selected." << std::endl;
						startSeason = -1;
						endSeason = -1;
						sleep(2);
					}
				}
				else
				{
					try
					{ // Is seasons a single number?
						startSeason = std::stoi(seasons);
						endSeason = std::stoi(seasons);
					}
					catch (std::exception &e)
					{
						std::cerr << "Error: Can not decipher season range, " <<
							"defaulting to selected." << std::endl;
						sleep(2);
					}
				}
				break;
			case 'f':	// Format
				if (strncmp(optarg, "raw", 3) == 0)
					playlist = PL_RAW;
				else if (strncmp(optarg, "m3u", 3) == 0)
					playlist = PL_M3U;
				else if (strncmp(optarg, "pls", 3) == 0)
					playlist = PL_PLS;
				else
					std::cerr << "Playlist format not recognized, defaulting to raw." << std::endl;
				break;
			case 'y':	// youtube-dl
				direct_url = true;
				break;
			case 'V':	// Version
				std::cout << "seriespl " << version << "\n"
						  << "Copyright © 2016 tastytea <tastytea@tastytea.de>.\n"
						  << "License GPLv2: GNU GPL version 2 <http://www.gnu.org/licenses/gpl-2.0.html>.\n"
						  << "This is free software: you are free to change and redistribute it.\n"
						  << "There is NO WARRANTY, to the extent permitted by law." << std::endl;
				return 0;
				break;
			default:
				std::cerr << usage << std::endl;
				return 1;
				break;
		}
	}
	if (optind >= argc)
	{
		std::cerr << usage << std::endl;
		return 1;
	}
	else
	{ // Set URL of tv series
		directoryurl = argv[optind];
	}

	return 0;
}

void Seriespl::populate_providers(const std::string &providerlist)
{
	std::istringstream ss;

	ss.str(providerlist);
	Providers.clear();

	std::regex reConfig("([[:alnum:]]+)");
	std::sregex_iterator it_re(providerlist.begin(), providerlist.end(), reConfig);
	std::sregex_iterator it_re_end;

	while (it_re != it_re_end)
	{
		std::map<StreamProviders, providerpair>::const_iterator it;
		for (it = providermap.begin(); it != providermap.end(); ++it)
		{
			if (it->second.first == (*it_re)[1])
				Providers.push_back(it->first);
		}
		++it_re;
	}
	if (Providers.empty())
	{
		std::cerr << "Error: List of streaming providers is empty." << std::endl;
		exit(2);
	}
}

std::string Seriespl::getlink(const std::string &url, const StreamProviders &provider,
							  std::string &title)
{ // Takes URL of episode-page and streaming provider, returns URL of stream-page or "" on error
	std::string content = getpage(url);
	std::string streamurl = "";

	if (service == BurningSeries)
	{
		std::regex reStreamPage("(<a href| src)=[\"\'](https?://" +
			providermap.at(provider).second + "/.*)[\"\']( target=|></iframe>)");
		std::regex reTitle(
			std::string("<h2 id=\"titleGerman\">(.*)") +
				"[[:space:]]+<small id=\"titleEnglish\" lang=\"en\">(.*)</small>");
		std::smatch match;

		if (std::regex_search(content, match, reStreamPage))
		{
			streamurl = match[2].str();
			
			if (provider == Streamcloud ||
				provider == Vivo ||
				provider == Shared ||
				provider == YouTube)
			{ // Make sure we use SSL where supported
				if (streamurl.find("https") == std::string::npos)
				{
					streamurl = "https" + streamurl.substr(4, std::string::npos);
				}
			}
		}
		else
		{
			std::cerr << "Error extracting stream" << std::endl;
		}

		if (std::regex_search(content, match, reTitle))
		{
			if (match[1].str() != "")		// German
				title = match[1].str();
			else if (match[2].str() != "")	// English
				title = match[2].str();
		}
	} // service-if

	return streamurl;
}

std::string Seriespl::getlink(const std::string &url, const StreamProviders &provider)
{
	std::string title;
	return getlink(url, provider, title);
}

void Seriespl::print_playlist(const PlaylistFormat &playlist, const std::string &url,
							  const std::string &title)
{
	static unsigned short counter = 1;

	switch (playlist)
	{
		case PL_RAW:
			std::cout << url << std::endl;
			break;
		case PL_M3U:
			if (counter == 1) // Write header
			{
				std::cout << "#EXTM3U" << std::endl;
			}
			std::cout << "#EXTINF:-1," << title << std::endl;
			std::cout << url << std::endl;
			break;
		case PL_PLS:
			if (counter == 1) // Write header
			{
				std::cout << "[playlist]" << std::endl;
				std::cout << "Version=2" << std::endl;
			}
			if (url == "NUMBER_OF_EPISODES")
			{
				std::cout << "NumberOfEntries=" << counter - 1 << std::endl;
			}
			else
			{
				std::cout << "File" << counter << "=" << url << std::endl;
				std::cout << "Title" << counter << "=" << title << std::endl;
				std::cout << "Length" << counter << "=-1" << std::endl;
			}
			break;
	}
	++counter;
}

void Seriespl::print_playlist(const PlaylistFormat &playlist, const std::string &url)
{
	return print_playlist(playlist, url, "");
}

std::string Seriespl::get_direct_url(std::string &providerurl)
{ // Use youtube-dl to print the direct URL of the video file
	FILE *ytdl;
	char buffer[256];
	std::string result;

	if(!(ytdl = popen(( yt_dl_path + " --get-url " + providerurl).c_str(), "r")))
	{
		std::cerr << "Error: Can not spawn process for youtube-dl" << std::endl;
		return "";
	}

	while(fgets(buffer, sizeof(buffer), ytdl) != NULL)
	{
		result += buffer;
	}
	if (pclose(ytdl) != 0)
	{
		std::cerr << "Error: youtube-dl returned non-zero exit code" << std::endl;
		return "";
	}

	return result.substr(0, result.find_last_not_of("\r\n") + 1);
}

//=======================================================================
// Copyright (c) 2014 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>

#include "graphs.hpp"

namespace graphs
{

  static
  std::unordered_map<std::string, std::unordered_map<std::string, std::size_t>>
  compute_values (const graph& g)
  {
    std::unordered_map<std::string, std::unordered_map<std::string, std::size_t>> results;

    std::for_each (g.begin (), g.end (), [&](const result& r) {
      results[r.group][r.series] = r.value;
    });

    return results;
  }

  static
  bool
  numeric_cmp (const std::string& lhs, const std::string& rhs)
  {
    return atoi (lhs.c_str ()) < atoi (rhs.c_str ());
  }

  graph::
  graph (const std::string& name, const std::string& title, const std::string& unit)
    : m_name (name),
      m_title (title),
      m_unit (unit)
  { }

  result&
  graph::
  add_result (const std::string& series, const std::string& group, std::size_t value)
  {
    std::cout << series << ":" << group << ":" << value << std::endl;
    m_results.push_back ({ series, group, value });
    return m_results.back ();
  }

  const std::string&
  graph::
  get_name (void) const noexcept
  {
    return m_name;
  }

  const std::string&
  graph::
  get_title (void) const noexcept
  {
    return m_title;
  }

  const std::string&
  graph::
  get_unit (void) const noexcept
  {
    return m_unit;
  }

  std::vector<result>::const_iterator
  graph::
  begin (void) const noexcept
  {
    return m_results.begin ();
  }

  std::vector<result>::const_iterator
  graph::
  end (void) const noexcept
  {
    return m_results.end ();
  }

  graph&
  graph_manager::
  add_graph (const std::string& graph_name, const std::string& graph_title,
             const std::string& unit)
  {
    std::cout << "Start " << graph_name << std::endl;
    m_graphs.emplace_back (graph_name, graph_title, unit);
    return m_graphs.back ();
  }

  std::vector<graph>::const_iterator
  graph_manager::
  begin (void) const noexcept
  {
    return m_graphs.begin ();
  }

  std::vector<graph>::const_iterator
  graph_manager::
  end (void) const noexcept
  {
    return m_graphs.end ();
  }

  void
  graph_manager::
  generate_output (output_type out) const
  {
    constexpr std::size_t graph_width  = 1200;
    constexpr std::size_t graph_height = 675;

    std::ofstream file ("graph.html");
    switch (out)
    {
      case output_type::GOOGLE:
        file << "<html>" << std::endl;
        file << "<head>" << std::endl;
        file << "<script type=\"text/javascript\" src=\"https://www.google.com/jsapi\"></script>"
             << std::endl;
        file << "<script type=\"text/javascript\">google.load('visualization', '1.0', {'packages':['corechart']});</script>"
             << std::endl;
        file << "</head>" << std::endl;
        file << "<body>" << std::endl;

        file << "<script type=\"text/javascript\">" << std::endl;

        //One function to rule them all
        std::for_each (begin (), end (), [&](const graph& g) {
          file << "function draw_" << g.get_name () << "(){" << std::endl;

          file << "var data = google.visualization.arrayToDataTable([" << std::endl;

          //['x', 'Cats', 'Blanket 1', 'Blanket 2'],
          auto results = compute_values (g);

          file << "['x'";

          auto first = results.begin ()->second;
          using series_map_pair_ref = std::unordered_map<std::string, std::size_t>::const_reference;
          std::for_each (first.begin (), first.end (), [&](series_map_pair_ref pair) {
            file << ", '" << pair.first << "'";
          });

          file << "]," << std::endl;

          std::vector<std::string> groups;
          using group_map_pair_ref = decltype(results)::const_reference;
          std::transform (results.begin (), results.end (), std::back_inserter (groups),
                          [](group_map_pair_ref pair) { return pair.first; });
          std::sort (groups.begin (), groups.end (), numeric_cmp);

          std::size_t max = 0;
          std::for_each (groups.begin (), groups.end (), [&](const std::string& group_title) {
            file << "['" << group_title << "'";

            const auto& series_map = results[group_title];
            std::for_each (series_map.begin (), series_map.end (), [&](series_map_pair_ref pair) {
              file << ", " << pair.second;
              max = std::max (max, pair.second);
            });

            file << "]," << std::endl;
          });

          file << "]);" << std::endl;

          file << "var graph = new google.visualization.LineChart(document.getElementById('graph_"
               << g.get_name () << "'));" << std::endl
               << "var options = {curveType: \"function\","
               << "title: \"" << g.get_title () << "\","
               << "animation: {duration:1200, easing:\"in\"},"
               << "width: " << graph_width << ", height: " << graph_height << ","
               << "chartArea: {left: 20, width: '75%'},"
               << "hAxis: {title:\"Number of elements\", slantedText:true},"
               << "vAxis: {viewWindow: {min:0}, title:\"" << g.get_unit () << "\"}};" << std::endl
               << "graph.draw(data, options);" << std::endl;

          file << "var button = document.getElementById('graph_button_" << g.get_name () << "');"
               << std::endl
               << "button.onclick = function(){" << std::endl
               << "if(options.vAxis.logScale){" << std::endl
               << "button.value=\"Logarithmic Scale\";" << std::endl
               << "} else {" << std::endl
               << "button.value=\"Normal scale\";" << std::endl
               << "}" << std::endl
               << "options.vAxis.logScale=!options.vAxis.logScale;" << std::endl
               << "graph.draw(data, options);" << std::endl
               << "};" << std::endl;

          file << "}" << std::endl;
        });

        //One function to find them
        file << "function draw_all(){" << std::endl;
        std::for_each (begin (), end (), [&](const graph& g) {
          file << "draw_" << g.get_name () << "();" << std::endl;
        });
        file << "}" << std::endl;

        //One callback to bring them all
        file << "google.setOnLoadCallback(draw_all);" << std::endl;
        file << "</script>" << std::endl;
        file << std::endl;

        //And in the web page bind them
        std::for_each (begin (), end (), [&](const graph& g) {
          file << "<div id=\"graph_" << g.get_name ()
               << "\" style=\"width: " << graph_width
               << "px; height: " << graph_height << "px;\"></div>" << std::endl;
          file << "<input id=\"graph_button_" << g.get_name ()
               << "\" type=\"button\" value=\"Logarithmic scale\">" << std::endl;
        });

        file << "</body>" << std::endl;
        file << "</html>" << std::endl;

        //...In the land of Google where shadow lies
        break;
      case output_type::PLUGIN:
        //One function to rule them all
        std::for_each (begin (), end (), [&](const graph& g) {
          file << "[line_chart width=\"" << graph_width
               << "px\" height=\"" << graph_height << "px\" "
               << "scale_button=\"true\" "
               << "title=\"" << g.get_title ()
               << "\" h_title=\"Number of elements\" v_title=\"" << g.get_unit () << "\"]"
               << std::endl;

          //['x', 'Cats', 'Blanket 1', 'Blanket 2'],
          auto results = compute_values (g);

          file << "['x'";

          auto first = results.begin ()->second;
          using series_map_pair_ref = std::unordered_map<std::string, std::size_t>::const_reference;
          std::for_each (first.begin (), first.end (), [&](series_map_pair_ref pair) {
            file << ", '" << pair.first << "'";
          });

          file << "]," << std::endl;

          std::vector<std::string> groups;
          using group_map_pair_ref = decltype(results)::const_reference;
          std::transform (results.begin (), results.end (), std::back_inserter (groups),
                          [](group_map_pair_ref pair) { return pair.first; });
          std::sort (groups.begin (), groups.end (), numeric_cmp);

          std::size_t max = 0;
          std::for_each (groups.begin (), groups.end (), [&](const std::string& group_title) {
            file << "['" << group_title << "'";

            const auto& series_map = results[group_title];
            std::for_each (series_map.begin (), series_map.end (), [&](series_map_pair_ref pair) {
              file << ", " << pair.second;
              max = std::max (max, pair.second);
            });

            file << "]," << std::endl;
          });

          file << "[/line_chart]" << std::endl;
        });
        break;
#ifndef __clang__
      default:
        break;
#endif
    }
  }

}

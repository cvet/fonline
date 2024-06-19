//=======================================================================
// Copyright (c) 2014 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#ifndef ARTICLES_GRAPHS
#define ARTICLES_GRAPHS

#include <memory>
#include <string>
#include <vector>


namespace graphs
{

  struct result
  {
    std::string series;
    std::string group;
    std::size_t value;
  };

  class graph
  {
  public:
    graph (const std::string& name, const std::string& title, const std::string& unit);

    result&
    add_result (const std::string& series, const std::string& group, std::size_t value);

    const std::string&
    get_name (void) const noexcept;

    const std::string&
    get_title (void) const noexcept;

    const std::string&
    get_unit (void) const noexcept;

    std::vector<result>::const_iterator
    begin (void) const noexcept;

    std::vector<result>::const_iterator
    end (void) const noexcept;

  private:
    std::string         m_name;
    std::string         m_title;
    std::string         m_unit;
    std::vector<result> m_results;
  };

  class graph_manager
  {
  public:
    enum class output_type
    {
      GOOGLE,
      PLUGIN
    };

    graph&
    add_graph (const std::string& graph_name, const std::string& graph_title,
               const std::string& unit);

    std::vector<graph>::const_iterator
    begin (void) const noexcept;

    std::vector<graph>::const_iterator
    end (void) const noexcept;

    void
    generate_output (output_type out) const;

  private:
    std::vector<graph> m_graphs;
  };
}

#endif

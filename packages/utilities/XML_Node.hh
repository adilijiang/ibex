#ifndef XML_Node_hh
#define XML_Node_hh

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "pugixml.hh"

#include "Check.hh"
#include "XML_Functions.hh"

#define XML_PRECISION 16

/*
  Interface for pugi::xml_node class
*/
class XML_Node
{
public:

    // Tests whether node exists
    inline operator bool() const
    {
        return *xml_node_;
    }

    // Get name
    std::string name() const
    {
        return name_;
    }
    
    // Find a child node
    XML_Node get_child(std::string name,
                       bool check = true);
    
    // Find multiple child nodes
    XML_Node get_sibling(std::string name,
                         bool check = true);
    
    // Append a child node
    XML_Node append_child(std::string name);

    // Get an attribute of the node, insisting that it exists
    template<typename T> T get_attribute(std::string description);

    // Get an attribute of the node given a default
    template<typename T> T get_attribute(std::string description,
                                         T def);

    // Get an attribute of the node, insisting that it exists
    template<typename T> std::vector<T> get_attribute_vector(std::string description);

    // Get an attribute of the node given a default
    template<typename T> std::vector<T> get_attribute_vector(std::string description,
                                                             std::vector<T> def);
    
    // Get the value of the node, insisting that it exists
    template<typename T> T get_value();
    template<typename T> std::vector<T> get_vector();
    template<typename T> std::vector<T> get_vector(int expected_size);
    template<typename T> std::vector<std::vector<T> > get_matrix(int expected_size_1,
                                                                 int expected_size_2);
    
    // Get the value of the node given a default
    template<typename T> T get_value(T def);
    template<typename T> std::vector<T> get_vector(int expected_size,
                                                   std::vector<T> def);
    template<typename T> std::vector<std::vector<T> > get_matrix(int expected_size_1,
                                                                 int expected_size_2,
                                                                 std::vector<std::vector<T> > def);
    
    // Get a child value of the node, insisting that it exists
    template<typename T> T get_child_value(std::string description);
    template<typename T> std::vector<T> get_child_vector(std::string description,
                                                         int expected_size);
    template<typename T> std::vector<std::vector<T> > get_child_matrix(std::string description,
                                                                       int expected_size_1,
                                                                       int expected_size_2);
    
    // Get a child value of the node given a default
    template<typename T> T get_child_value(std::string description,
                                           T def);
    template<typename T> std::vector<T> get_child_vector(std::string description,
                                                         int expected_size,
                                                         std::vector<T> def);
    template<typename T> std::vector<std::vector<T> > get_child_matrix(std::string description,
                                                                       int expected_size_1,
                                                                       int expected_size_2,
                                                                       std::vector<std::vector<T> > def);
    
    // Set attribute of node
    template<typename T> void set_attribute(T data,
                                            std::string description);
    
    // Set value of node
    template<typename T> void set_value(T data);
    template<typename T> void set_vector(std::vector<T> const &data,
                                         std::string index_order = "");
    template<typename T> void set_matrix(std::vector<std::vector<T> > const &data,
                                         std::string index_order = "");

    // Set value of child node
    template<typename T> void set_child_value(T data,
                                              std::string description);
    template<typename T> void set_child_vector(std::vector<T> const &data,
                                               std::string description,
                                               std::string index_order = "");
    template<typename T> void set_child_matrix(std::vector<std::vector<T> > const &data,
                                               std::string description,
                                               std::string index_order = "");

    // Append/prepend all values from one node to another
    void append_all(XML_Node copy_node);
    void prepend_all(XML_Node copy_node);

    // Add a single node to the tree
    void append_node(XML_Node copy_node);
    void prepend_node(XML_Node copy_node);
    
protected:
    
    // Create an XML_Node (see XML_Document for public creation)
    XML_Node(std::shared_ptr<pugi::xml_node> node,
             std::string name);

    std::shared_ptr<pugi::xml_node> xml_node()
    {
        return xml_node_;
    }
    
private:
    
    std::shared_ptr<pugi::xml_node> xml_node_;
    std::string name_;
};

/*
  Definitions for templated functions
*/
template<typename T> T XML_Node::
get_attribute(std::string description)
{
    pugi::xml_attribute attr = xml_node_->attribute(description.c_str());

    if (attr.empty())
    {
        std::string error_message
            = "required attribute ("
            + description
            + ") in node ("
            + name_
            + ") not found";
        
        AssertMsg(false, error_message);
    }

    return XML_Functions::attr_value<T>(attr);
}

template<typename T> T XML_Node::
get_attribute(std::string description,
              T def)
{
    pugi::xml_attribute attr = xml_node_->attribute(description.c_str());

    if (attr.empty())
    {
        return def;
    }

    return XML_Functions::attr_value<T>(attr);
}

template<typename T> std::vector<T> XML_Node::
get_attribute_vector(std::string description)
{
    pugi::xml_attribute attr = xml_node_->attribute(description.c_str());

    if (attr.empty())
    {
        std::string error_message
            = "required attribute ("
            + description
            + ") in node ("
            + name_
            + ") not found";
        
        AssertMsg(false, error_message);
    }

    return XML_Functions::attr_vector<T>(attr);
}

template<typename T> std::vector<T> XML_Node::
get_attribute_vector(std::string description,
                     std::vector<T> def)
{
    pugi::xml_attribute attr = xml_node_->attribute(description.c_str());

    if (attr.empty())
    {
        return def;
    }

    return XML_Functions::attr_vector<T>(attr);
}

template<typename T> T XML_Node::
get_value()
{
    pugi::xml_text text = xml_node_->text();
    
    if (text.empty())
    {
        std::string error_message
            = "required value in node ("
            + name_
            + ") not found";
        
        AssertMsg(false, error_message);
    }
    
    return XML_Functions::text_value<T>(text);
}

template<typename T> std::vector<T> XML_Node::
get_vector()
{
    pugi::xml_text text = xml_node_->text();

    if (text.empty())
    {
        std::string error_message
            = "required value in node ("
            + name_
            + ") not found";
        
        AssertMsg(false, error_message);
    }
    
    return XML_Functions::text_vector<T>(text);
}

template<typename T> std::vector<T> XML_Node::
get_vector(int expected_size)
{
    pugi::xml_text text = xml_node_->text();

    if (text.empty())
    {
        std::string error_message
            = "required value in node ("
            + name_
            + ") not found";
        
        AssertMsg(false, error_message);
    }
    
    std::vector<T> value = XML_Functions::text_vector<T>(text);

    if (value.size() != expected_size)
    {
        std::string error_message
            = "num values of in node ("
            + name_
            + ") incorrect; expected ("
            + std::to_string(expected_size)
            + ") but calculated ("
            + std::to_string(value.size())
            + ")";
        
        AssertMsg(false, error_message);
    }

    return value;
}

template<typename T> std::vector<std::vector<T> > XML_Node::
get_matrix(int expected_size_1,
           int expected_size_2)
{
    pugi::xml_text text = xml_node_->text();

    if (text.empty())
    {
        std::string error_message
            = "required value in node ("
            + name_
            + ") not found";
        
        AssertMsg(false, error_message);
    }
    
    std::vector<T> input = XML_Functions::text_vector<T>(text);

    if (input.size() != expected_size_1 * expected_size_2)
    {
        std::string error_message
            = "size in node ("
            + name_
            + ") incorrect - expected ("
            + std::to_string(expected_size_1 * expected_size_2)
            + ") but calculated ("
            + std::to_string(input.size())
            + ")";

        AssertMsg(false, error_message);
    }
    
    std::vector<std::vector<T> > value(expected_size_1, std::vector<T>(expected_size_2));

    for (int i = 0; i < expected_size_1; ++i)
    {
        for (int j = 0; j < expected_size_2; ++j)
        {
            value[i][j] = input[j + expected_size_2 * i];
        }
    }
    
    return value;
}

template<typename T> T XML_Node::
get_value(T def)
{
    pugi::xml_text text = xml_node_->text();

    if (text.empty())
    {
        return def;
    }

    return XML_Functions::text_value<T>(text);
}

template<typename T> std::vector<T> XML_Node::
get_vector(int expected_size,
           std::vector<T> def)
{
    pugi::xml_text text = xml_node_->text();

    if (text.empty())
    {
        return def;
    }

    std::vector<T> value = XML_Functions::text_vector<T>(text);

    if (value.size() != expected_size)
    {
        std::string error_message
            = "size in node ("
            + name_
            + ") incorrect - expected ("
            + std::to_string(expected_size)
            + ") but calculated ("
            + std::to_string(value.size())
            + ") - reverting to default value";
        std::cout << error_message << std::endl;

        return def;
    }
    
    return value;
}

template<typename T> std::vector<std::vector<T> > XML_Node::
get_matrix(int expected_size_1,
           int expected_size_2,
           std::vector<std::vector<T> > def)
{
    pugi::xml_text text = xml_node_->text();

    if (text.empty())
    {
        return def;
    }
    
    std::vector<T> input = XML_Functions::text_vector<T>(text);

    if (input.size() != expected_size_1 * expected_size_2)
    {
        std::string error_message
            = "size in node ("
            + name_
            + ") incorrect - expected ("
            + std::to_string(expected_size_1 * expected_size_2)
            + ") but calculated ("
            + std::to_string(input.size())
            + ") - reverting to default value";
        std::cout << error_message << std::endl;

        return def;
    }
    
    std::vector<std::vector<T> > value(expected_size_1, std::vector<T>(expected_size_2));

    for (int i = 0; i < expected_size_1; ++i)
    {
        for (int j = 0; j < expected_size_2; ++j)
        {
            value[i][j] = input[j + expected_size_2 * i];
        }
    }

    return value;
}

template<typename T> T XML_Node::
get_child_value(std::string description)
{
    return get_child(description,
                     false).get_value<T>();
}

template<typename T> std::vector<T> XML_Node::
get_child_vector(std::string description,
                 int expected_size)
{
    return get_child(description,
                     false).get_vector<T>(expected_size);
}

template<typename T> std::vector<std::vector<T> > XML_Node::
get_child_matrix(std::string description,
                 int expected_size_1,
                 int expected_size_2)
{
    return get_child(description,
                     false).get_matrix<T>(expected_size_1,
                                          expected_size_2);
}

template<typename T> T XML_Node::
get_child_value(std::string description,
                T def)
{
    return get_child(description,
                     false).get_value(def);
}

template<typename T> std::vector<T> XML_Node::
get_child_vector(std::string description,
                 int expected_size,
                 std::vector<T> def)
{
    return get_child(description,
                     false).get_vector(expected_size,
                                       def);
}

template<typename T> std::vector<std::vector<T> > XML_Node::
get_child_matrix(std::string description,
                 int expected_size_1,
                 int expected_size_2,
                 std::vector<std::vector<T> > def)
{
    return get_child(description,
                     false).get_matrix(expected_size_1,
                                       expected_size_2,
                                       def);
}

template<typename T> void XML_Node::
set_attribute(T data,
              std::string description)
{
    std::string data_string;
    String_Functions::to_string(data_string,
                                data,
                                XML_PRECISION);
    
    pugi::xml_attribute attr = xml_node_->append_attribute(description.c_str());
    attr.set_value(data_string.c_str());
}

template<typename T> void XML_Node::
set_value(T data)
{
    std::string data_string;
    String_Functions::to_string(data_string,
                                data,
                                XML_PRECISION);
    
    xml_node_->append_child(pugi::node_pcdata).set_value(data_string.c_str());
}

template<typename T> void XML_Node::
set_vector(std::vector<T> const &data,
           std::string index_order)
{
    std::string data_string;
    String_Functions::vector_to_string(data_string,
                                       data,
                                       XML_PRECISION);
    
    xml_node_->append_child(pugi::node_pcdata).set_value(data_string.c_str());

    if (index_order != "")
    {
        set_attribute(index_order,
                      "index");
    }
}

template<typename T> void XML_Node::
set_matrix(std::vector<std::vector<T> > const &data,
           std::string index_order)
{
    std::string data_string;
    for (std::vector<T> const &local_data : data)
    {
        std::string local_string;
        String_Functions::vector_to_string(local_string,
                                           local_data,
                                           XML_PRECISION);
        data_string.append(local_string);
    }
    
    xml_node_->append_child(pugi::node_pcdata).set_value(data_string.c_str());
    
    if (index_order != "")
    {
        set_attribute(index_order,
                      "index");
    }
}


template<typename T> void XML_Node::
set_child_value(T data,
                std::string description)
{
    append_child(description).set_value(data);
}

template<typename T> void XML_Node::
set_child_vector(std::vector<T> const &data,
                 std::string description,
                 std::string index_order)
{
    append_child(description).set_vector(data,
                                         index_order);
}

template<typename T> void XML_Node::
set_child_matrix(std::vector<std::vector<T> > const &data,
                 std::string description,
                 std::string index_order)
{
    append_child(description).set_matrix(data,
                                         index_order);
}

#endif

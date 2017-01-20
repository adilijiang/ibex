#include "Material_Parser.hh"

#include "Angular_Discretization.hh"
#include "Cross_Section.hh"
#include "Energy_Discretization.hh"
#include "Material.hh"
#include "XML_Node.hh"

using namespace std;

Material_Parser::
Material_Parser(shared_ptr<Angular_Discretization> angular,
                shared_ptr<Energy_Discretization> energy):
    angular_(angular),
    energy_(energy)
{
}

vector<shared_ptr<Material> > Material_Parser::
parse_from_xml(XML_Node input_node)
{
    int number_of_materials = input_node.get_child_value<int>("number_of_materials");
    
    int number_of_moments = angular_->number_of_scattering_moments();
    int number_of_groups = energy_->number_of_groups();
    
    // Parse the data for each material
    
    vector<shared_ptr<Material> > materials(number_of_materials);
    
    int checksum = 0;
    for (XML_Node material_node = input_node.get_child("material");
         material_node;
         material_node = material_node.get_sibling("material",
                                                   false))
    {
        int a = material_node.get_attribute<int>("index");
        
        shared_ptr<Cross_Section> sigma_t
            = make_shared<Cross_Section>(Cross_Section::Angular_Dependence::NONE,
                                         Cross_Section::Energy_Dependence::GROUP,
                                         angular_,
                                         energy_,
                                         material_node.get_child_vector<double>("sigma_t", number_of_groups));
        shared_ptr<Cross_Section> sigma_s
            = make_shared<Cross_Section>(Cross_Section::Angular_Dependence::SCATTERING_MOMENTS,
                                         Cross_Section::Energy_Dependence::GROUP_TO_GROUP,
                                         angular_,
                                         energy_,
                                         material_node.get_child_vector<double>("sigma_s", number_of_groups * number_of_groups * number_of_moments));
        shared_ptr<Cross_Section> nu
            = make_shared<Cross_Section>(Cross_Section::Angular_Dependence::NONE,
                                         Cross_Section::Energy_Dependence::GROUP,
                                         angular_,
                                         energy_,
                                         material_node.get_child_vector<double>("nu", number_of_groups));
        shared_ptr<Cross_Section> sigma_f
            = make_shared<Cross_Section>(Cross_Section::Angular_Dependence::NONE,
                                         Cross_Section::Energy_Dependence::GROUP,
                                         angular_,
                                         energy_,
                                         material_node.get_child_vector<double>("sigma_f", number_of_groups));
        shared_ptr<Cross_Section> chi
            = make_shared<Cross_Section>(Cross_Section::Angular_Dependence::NONE,
                                         Cross_Section::Energy_Dependence::GROUP,
                                         angular_,
                                         energy_,
                                         material_node.get_child_vector<double>("chi", number_of_groups));
        shared_ptr<Cross_Section> internal_source
            = make_shared<Cross_Section>(Cross_Section::Angular_Dependence::NONE,
                                         Cross_Section::Energy_Dependence::GROUP,
                                         angular_,
                                         energy_,
                                         material_node.get_child_vector<double>("internal_source", number_of_groups));
        
        shared_ptr<Material> material = make_shared<Material>(a,
                                                              angular_,
                                                              energy_,
                                                              sigma_t,
                                                              sigma_s,
                                                              nu,
                                                              sigma_f,
                                                              chi,
                                                              internal_source);
        
        materials[a] = material;
        
        checksum += a;
    } // materials
    
    int checksum_expected = number_of_materials * (number_of_materials - 1) / 2;
    AssertMsg(checksum == checksum_expected, "Material indexing incorrect");

    return materials;
}

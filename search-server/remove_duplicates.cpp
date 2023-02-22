#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) 
{ 
    
  std::map<std::set<std::string>, int> dupl; 
  std::set<std::string> unique; 
  std::set<int> id_delete; 
 

  for (const int document_id : search_server)  
  { 
    std::map<std::string, double> array = search_server.GetWordFrequencies(document_id); 
 

    transform(begin(array), end(array), inserter(unique, unique.begin()), [](auto String) { return String.first; }); 
 

    if(!dupl.count(unique)) 
    { 
      dupl.emplace(unique, document_id); 
    } 
    else 
    { 
      id_delete.emplace(document_id); 
    } 
    unique.clear(); 
  } 
  for (auto& id : id_delete) 
  { 
    std::cout << "Found duplicate document id " << id << std::endl; 
    search_server.RemoveDocument(id); 
  } 
}

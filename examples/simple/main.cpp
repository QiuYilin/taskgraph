#include "taskgraph.hpp"
#include "AddNode.hpp"
#include "IntNode.hpp"
#include "Ui.hpp"
int main(){
    tg::Controller project;
    auto graph = std::make_shared<tg::Graph>();
    project.addGraph(graph);

    Ui ui(project);
    ui.addNodeKind("Int", [] { return std::make_shared<tg::IntNode>(); });
    ui.addNodeKind("Add", [] { return std::make_shared<tg::AddNode>(); });
    ui.Run();
    return 0;
}

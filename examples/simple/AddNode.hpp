#pragma once
#include "Node.hpp"
#include "IntNodeData.hpp"
namespace tg{
    class AddNode: public Node{
    public:
        AddNode();
        //TODO public methods
        void compute() override;
        void setInPortData(unsigned int index,std::shared_ptr<NodeData> data) override;
        std::shared_ptr<NodeData> getOutPortData(unsigned int) override;

    private:
        std::weak_ptr<NodeData> _in_data1,_in_data2;
        std::shared_ptr<NodeData> _out_data;
    };
}
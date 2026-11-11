#pragma once
#include "PclNode.hpp"
namespace tg{
    class PointCloudReaderNode: public PclNode{
    public:
        PointCloudReaderNode();
        unsigned int nFixedPorts(PortType port_type) const override;
        NodeDataType fixedDataType(PortType port_type, unsigned int index) const override;
        void compute() override;
        void setInPortData(unsigned int index,std::shared_ptr<NodeData> data) override;
        std::shared_ptr<NodeData> getOutPortData(unsigned int) override;
        //void setProperty(int property);
        void setFilePathProp(std::string file_path);

    private:
        using PclNode::_pointcloud_out;
        using PclNode::_image_type;
        //用什么容器保存属性？？
        std::shared_ptr<Property> _file_path;
    };
}//namespace tg
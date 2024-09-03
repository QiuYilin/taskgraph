//创建预制的模板 输入模板图片和掩膜图片（提供掩膜工具？）

#pragma once
#include "CvNode.hpp"
namespace tg{
    class ImageReaderNode: public CvNode{
    public:
        ImageReaderNode();
        ~ImageReaderNode()=default;
        void compute() override;
        void setInPortData(unsigned int index,std::shared_ptr<NodeData> data) override;
        std::shared_ptr<NodeData> getOutPortData(unsigned int) override;
        //void setProperty(int property);
        void setFilePathProp(std::string file_path);

    private:
        using CvNode::_image_out;
        //using CvNode::_image_type;
        std::shared_ptr<Property> _file_path;
    };
}
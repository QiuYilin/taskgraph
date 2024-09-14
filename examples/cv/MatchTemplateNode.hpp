//创建预制的模板 输入模板图片和掩膜图片（提供掩膜工具？）

#pragma once
#include "CvNode.hpp"
namespace tg{
    class MatchTemplateNode: public CvNode{
    public:
        MatchTemplateNode();
        void compute() override;
        void setInPortData(unsigned int index,std::shared_ptr<NodeData> data) override;
        std::shared_ptr<NodeData> getOutPortData(unsigned int) override;
        //void setProperty(int property);

    private:
        using CvNode::_image_out;
        std::weak_ptr<NodeData> _image_in;
        std::weak_ptr<NodeData> _image_tml;
        std::weak_ptr<NodeData> _image_mask;
        std::shared_ptr<NodeData> _loc_out;//TODO 先写个最简单的模板匹配看看框架
        std::shared_ptr<Property> _pyramid_layers;
    };
}
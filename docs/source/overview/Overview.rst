
概述
=======

简介
-------

CNStream是面向寒武纪开发平台的数据流处理SDK。用户可以根据CNStream提供的接口，开发实现自己的组件。还可以通过组件之间的互连，灵活地实现自己的业务需求。CNStream能够大大简化寒武纪深度学习平台提供的推理和其他处理，如视频解码、神经网络图像前处理的集成。也能够在兼顾灵活性的同时，充分发挥寒武纪MLU（Machine Learning Unit 机器学习处理器）的硬件解码和机器学习算法的运算性能。

CNStream基于模块化和流水线的思想，提供了一套基于C++语言的接口来支持流处理多路并发的Pipeline框架。为用户提供可自定义的模块机制以及通过高度抽象的DataFrame类型进行模块间的数据传输，满足用户对性能和可伸缩性的需求。

CNStream支持在MLU270和MLU220 M.2平台上使用。

CNStream特点
--------------

CNStream构建了一整套寒武纪硬件平台上的实时数据流分析框架。框架具有多个基于寒武纪思元处理器的硬件加速模块，可将深层神经网络和其他复杂处理任务带入流处理管道。开发者只需专注于构建核心深度学习网络和知识产权（Intellectual Property，IP），并用寒武纪特定模型转换器生成寒武纪平台能执行的模型，无需再从头开始设计端到端的解决方案。

CNStream具有以下几个特点:

* 简单易用的模块化设计。内置模块及正在扩充的模块库可以让用户快速构建自己的业务应用，无需关心实现细节。

* 高效的流水线设计。区别于Gstreamer等框架庞大的结构及传统的视频处理流水线结构，CNStream设计了一套伸缩灵活的流水线框架。每一个模块的并行度及队列深度可以根据时延要求及数据吞吐量而定制。根据业务需求，通过配置JSON文件就可以很方便地进行调整。

* 丰富的原生模块。为提高推理效率，根据寒武纪神经网络推理芯片设计特点，内置了从数据源解码、前后处理及推理、追踪等模块。其中推理模块、前后处理模块、编解码模块和追踪模块充分利用了寒武纪芯片设计特点和内部IP核心间DMA的能力,使用后极大的提高了系统整体吞吐效率。

* 支持分布式架构，内置Kafka消息模块，帮助客户快速接入分布式系统。

* 支持常见分类以及常规目标检测神经网络，比如：YOLO、SSD、ResNet、VGG等。高效支持低位宽(INT8)和稀疏权值运算。

* 图形化界面方式生成pipeline。整个框架支持图形化拖拽方式生成pipeline，帮助用户快速体验寒武纪数据流分析框架。

* 灵活布署。使用标准C++ 11开发，可以将一份代码根据设备能力，在云端和边缘侧集成。


针对常规视频结构化分析（IVA）领域，使用CNStream开发应用可以带来如下便捷：

* 数据流处理能力。具有少冗余、高效率的特点。

* 由于TensorFlow、Caffe等深度学习框架，强调框架算子的完整性及快速验证深度学习模型的能力，在生产系统中无法利用硬件性能达到最高效率。然而，CNStream内置CNInfer模块直接基于寒武纪运行时库（CNRT）设计，能够高效利用底层硬件能力，快速完成组件的开发。

* 内置Tracker模块提供了经过寒武纪芯片加速优化后的KCF算法，使多路并行比运行在CPU上的Deepsort效率更好。

* 良好的本地化支持团队、持续的迭代开发能力以及内建开发者社区提供了快速的客户响应服务。


CNStream应用框架
-----------------------------

使用CNStream SDK开发一个应用的典型框架如下图：

    .. figure::  ../images/detection_flow.png

       典型视频结构化场景架构图 

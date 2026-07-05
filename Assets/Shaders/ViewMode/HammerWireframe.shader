{
    "Source": "HammerWireframe.azsl",
    "DepthStencilState": {
        "Depth": {
            "Enable": true,
            "WriteMask": "Zero",
            "CompareFunc": "GreaterEqual"
        }
    },
    "RasterState": {
        "FillMode": "Wireframe",
        "CullMode": "None",
        "DepthBias": -10,
        "DepthBiasSlopeScale": -1.0
    },
    "DrawList": "hammerwireframe",
    "ProgramSettings": {
        "EntryPoints": [
            {
                "name": "MainVS",
                "type": "Vertex"
            },
            {
                "name": "MainPS",
                "type": "Fragment"
            }
        ]
    }
}

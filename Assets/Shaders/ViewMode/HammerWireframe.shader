{
    "Source": "HammerWireframe.azsl",
    "DepthStencilState": {
        "Depth": {
            "Enable": false,
            "WriteMask": "Zero"
        }
    },
    "RasterState": {
        "FillMode": "Wireframe",
        "CullMode": "None"
    },
    "GlobalTargetBlendState": {
        "Enable": true,
        "BlendSource": "AlphaSource",
        "BlendDest": "AlphaSourceInverse",
        "BlendOp": "Add"
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

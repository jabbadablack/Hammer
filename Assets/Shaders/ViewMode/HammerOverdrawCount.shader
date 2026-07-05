{
    "Source": "HammerOverdrawCount.azsl",
    "DepthStencilState": {
        "Depth": {
            "Enable": true,
            "WriteMask": "Zero",
            "CompareFunc": "GreaterEqual"
        }
    },
    "RasterState": {
        "CullMode": "Back"
    },
    "DrawList": "hammeroverdraw",
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

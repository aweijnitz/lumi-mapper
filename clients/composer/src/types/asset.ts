export type AssetType = "VideoFile" | "ImageFile";

export type AssetVariant = {
  path: string;
  note: string;
};

export type Asset = {
  id: string;
  name: string;
  type: AssetType;
  path: string;
  variants: AssetVariant[];
};


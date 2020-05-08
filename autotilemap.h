#ifndef AUTOTILEMAP_H
#define AUTOTILEMAP_H

#include "core/reference.h"
#include "core/vector.h"
#include "core/dictionary.h"
#include "core/variant.h"
#include "core/vector.h"
#include "scene/2d/tile_map.h"
#include <string>
#include <vector>
#include "core/io/json.h"

class Autotiler : public Reference {
public:

	virtual bool is_source_tile(int tid) const {
		return _src_tile == tid;
	}

	virtual bool is_neighbor_tile(int tid) const {
		for (int i = 0; i < _neighbor_tile_ids.size(); i++) {
			if (tid == _neighbor_tile_ids[i]) {
				return true;
			}
		}
		return false;		
	}

	int get_src_tile() {
		return _src_tile;
	}

	virtual String get_type() const = 0;
	
	bool is_type(const String& type) const {
		return type == get_type();
	}
	virtual int get_atlas_id() const {return 0;}

	Dictionary& get_metadata_map() {
		return _tile_metadata_map;
	}
	
protected:
	Dictionary _tile_metadata_map;
	Vector<int> _neighbor_tile_ids;
	int _src_tile = 0;
};

class BlobTerrainAutotiler : public Autotiler {
public:

	String get_type() const {
		return "BlobTerrainAutotiler";
	}
	
	int get_atlas_id() const { return _atlas_id; }

	bool is_neighbor_tile(int tid) const {
		if (tid == _src_tile or tid == _comp_tile) {
			return true;
		}
		return false;
	}

	void init(const Variant& autotiler, const Vector<Variant>& codes) {
		_atlas_id = autotiler.get("atlas_id");
		_src_tile = autotiler.get("src_1");
		_src_2 = autotiler.get("src_2");
		_comp_tile = autotiler.get("comp_tile");
		for (int i = 0; i < codes.size(); i++) {
			auto code = codes[i];
			_tile_metadata_map[int(code.get("code"))] = int(code.get("id"));
		}
	}

	int get_src_1() {
		return _src_tile;
	}

	int get_src_2() {
		return _src_2;
	}

private:
	
	uint32_t _atlas_id = 0;
	uint32_t _src_2 = 0;
	uint32_t _comp_tile = 0;
};

class BlobAutotiler : public Autotiler {
public:
	String get_type() const {
		return "BlobAutotiler";
	}

	int get_atlas_id() const {return _atlas_id ;}
		
	void init(const Variant& autotiler, const Vector<Variant>& codes) {
		_atlas_id = autotiler.get("atlas_id");
		_src_tile = autotiler.get("src_tile");
		Variant neighbor_ids = autotiler.get("neighbor_tiles");
		Vector<int> neighbors = neighbor_ids;
		for (int i = 0; i < neighbors.size(); i++) {			
			_neighbor_tile_ids.push_back(neighbors[i]);
		}
		
		for (int i = 0; i < codes.size(); i++) {
			auto code = codes[i];
			_tile_metadata_map[int(code.get("code"))] = int(code.get("id"));
		}
	}	
	
private:
	
	uint32_t _atlas_id = 0;
};


class QuadAutotiler : public Autotiler {
public:

	/* struct Meta : public Object { */
	/* Meta(int c, bool fx, bool fy) : code(c), flip_x(fx), flip_y(fy) {} */
	/* 	int code; */
	/* 	bool flip_x; */
	/* 	bool flip_y; */
	/* }; */
	
	void init(const Variant& autotiler) {
		_src_tile = autotiler.get("src_tile");
		Vector<Variant> codes = autotiler.get("codes");
		for (int i = 0; i < codes.size(); i++) {
			auto code = codes[i];
			_tile_metadata_map[int(code.get("code"))] = code;//.get("id")), code.get("x_mirror"), code.get("y_mirror");
		}
		
		Variant neighbor_ids = autotiler.get("neighbor_tiles");
		Vector<int> neighbors = neighbor_ids;
		for (int i = 0; i < neighbors.size(); i++) {			
			_neighbor_tile_ids.push_back(neighbors[i]);
		}
	}
			  
	String get_type() const {
		return "QuadAutotiler";
	}	
};

class Autotilemap : public Reference {
	GDCLASS(Autotilemap, Reference);

protected:
    static void _bind_methods();
	
public:
	void init(const Vector2& top_left, const Vector2& bottom_right, const String& json_file);
	void load_from_json(const String& json_file);
	void apply(Object* obj_tilemap);
	void set_value(int x, int y, int v);
	void set_value_relative_to_tl(int x, int y, int v);
	void set_submap(Vector2 sub_top_left, int width, int height, const Variant& idata);
	Vector2 get_top_left();
	Vector2 get_bottom_right();
	int get_width();
	int get_height();
	Variant get_data();
		
	Autotilemap();

private:

	void apply_blob_autotiling(TileMap* tilemap);
	void apply_blob_terrain_autotiling(TileMap* tilemap);
	void map_ids_to_tiles(TileMap* tilemap);
	void apply_autotiling(TileMap* tilemap);
	
	Variant _json_data;
	Vector<Variant> _id_to_atlas;
	Vector2 _top_left;
	Vector2 _bottom_right;
	int _width = 0;
	int _height = 0;
	int _base_tile = 0;
	Vector<int32_t> _data;   //TypedArray instead?
	Vector<Ref<Autotiler>> _autotilers;
	bool _blob_mode = false;		
};

#endif

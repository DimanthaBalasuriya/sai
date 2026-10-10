#include "engine/core/SpatialIndex.hpp"
#include "engine/core/Profile.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace Engine::Spatial {
namespace {
Aabb Union(Aabb a,Aabb b){return {{std::min(a.minimum.x,b.minimum.x),std::min(a.minimum.y,b.minimum.y),std::min(a.minimum.z,b.minimum.z)},{std::max(a.maximum.x,b.maximum.x),std::max(a.maximum.y,b.maximum.y),std::max(a.maximum.z,b.maximum.z)}};}
float Area(Aabb a){auto x=a.maximum.x-a.minimum.x,y=a.maximum.y-a.minimum.y,z=a.maximum.z-a.minimum.z;return 2*(x*y+x*z+y*z);}
bool Contains(Aabb a,Aabb b){return a.minimum.x<=b.minimum.x&&a.minimum.y<=b.minimum.y&&a.minimum.z<=b.minimum.z&&a.maximum.x>=b.maximum.x&&a.maximum.y>=b.maximum.y&&a.maximum.z>=b.maximum.z;}
bool Overlaps(Aabb a,Aabb b){return a.minimum.x<=b.maximum.x&&a.minimum.y<=b.maximum.y&&a.minimum.z<=b.maximum.z&&a.maximum.x>=b.minimum.x&&a.maximum.y>=b.minimum.y&&a.maximum.z>=b.minimum.z;}
Aabb Fat(Aabb a){constexpr float margin=.1f;a.minimum.x-=margin;a.minimum.y-=margin;a.minimum.z-=margin;a.maximum.x+=margin;a.maximum.y+=margin;a.maximum.z+=margin;return a;}
bool Intersects(Aabb a,SpatialRay ray){float lo=0,hi=std::numeric_limits<float>::max();float minimum[]{a.minimum.x,a.minimum.y,a.minimum.z},maximum[]{a.maximum.x,a.maximum.y,a.maximum.z},o[]{ray.origin.x,ray.origin.y,ray.origin.z},d[]{ray.direction.x,ray.direction.y,ray.direction.z};for(int i=0;i<3;++i){if(std::abs(d[i])<1e-12f){if(o[i]<minimum[i]||o[i]>maximum[i])return false;continue;}auto t1=(minimum[i]-o[i])/d[i],t2=(maximum[i]-o[i])/d[i];if(t1>t2)std::swap(t1,t2);lo=std::max(lo,t1);hi=std::min(hi,t2);if(lo>hi)return false;}return true;}
bool Intersects(Aabb a,std::span<const Plane> planes){for(auto p:planes){auto x=p.normal.x>=0?a.maximum.x:a.minimum.x,y=p.normal.y>=0?a.maximum.y:a.minimum.y,z=p.normal.z>=0?a.maximum.z:a.minimum.z;if(p.normal.x*x+p.normal.y*y+p.normal.z*z+p.offset<0)return false;}return true;}
}
SpatialIndex::Proxy SpatialIndex::Allocate(){if(m_free!=-1){auto i=m_free;m_free=m_nodes[i].parent;m_nodes[i]=Node{};return i;}if(m_nodes.size()==m_nodes.capacity()){auto capacity=std::max<std::size_t>(64,m_nodes.size()*2);m_nodes.reserve(capacity);m_stack.reserve(capacity);}m_nodes.emplace_back();return static_cast<Proxy>(m_nodes.size()-1);}
void SpatialIndex::Free(Proxy i){m_nodes[i]=Node{};m_nodes[i].parent=m_free;m_free=i;}
void SpatialIndex::Clear(){m_nodes.clear();m_stack.clear();m_root=m_free=-1;m_size=0;m_reinserts=0;}
SpatialIndex::Proxy SpatialIndex::Insert(Aabb bounds,std::uint32_t payload){ENGINE_PROFILE_SCOPE("Scene.SpatialInsert");auto i=Allocate();m_nodes[i].bounds=Fat(bounds);m_nodes[i].payload=payload;m_nodes[i].height=0;Attach(i);++m_size;return i;}
void SpatialIndex::Remove(Proxy i){if(i<0||static_cast<std::size_t>(i)>=m_nodes.size()||m_nodes[i].height!=0)return;Detach(i);Free(i);--m_size;}
bool SpatialIndex::Update(Proxy i,Aabb bounds){ENGINE_PROFILE_SCOPE("Scene.SpatialUpdate");if(i<0||static_cast<std::size_t>(i)>=m_nodes.size()||m_nodes[i].height!=0)return false;if(Contains(m_nodes[i].bounds,bounds))return false;Detach(i);m_nodes[i].bounds=Fat(bounds);Attach(i);++m_reinserts;return true;}
void SpatialIndex::Fix(Proxy i){auto& n=m_nodes[i];n.bounds=Union(m_nodes[n.left].bounds,m_nodes[n.right].bounds);n.height=1+std::max(m_nodes[n.left].height,m_nodes[n.right].height);}
SpatialIndex::Proxy SpatialIndex::Balance(Proxy i){auto& a=m_nodes[i];if(a.height<2)return i;auto b=a.left,c=a.right;auto delta=m_nodes[c].height-m_nodes[b].height;if(std::abs(delta)<2)return i;auto root=delta>1?c:b;auto& r=m_nodes[root];r.parent=a.parent;if(a.parent!=-1){auto& p=m_nodes[a.parent];if(p.left==i)p.left=root;else p.right=root;}else m_root=root;a.parent=root;
 if(delta>1){auto f=r.left,g=r.right;r.left=i;if(m_nodes[f].height>m_nodes[g].height){r.right=f;a.right=g;m_nodes[g].parent=i;}else{r.right=g;a.right=f;m_nodes[f].parent=i;}}
 else{auto d=r.left,e=r.right;r.right=i;if(m_nodes[d].height>m_nodes[e].height){r.left=d;a.left=e;m_nodes[e].parent=i;}else{r.left=e;a.left=d;m_nodes[d].parent=i;}}
 Fix(i);Fix(root);return root;
}
void SpatialIndex::Attach(Proxy leaf){if(m_root==-1){m_root=leaf;return;}auto sibling=m_root;auto bounds=m_nodes[leaf].bounds;while(m_nodes[sibling].left!=-1){auto& n=m_nodes[sibling];auto combined=Area(Union(n.bounds,bounds));auto inheritance=2*(combined-Area(n.bounds));auto cost=[&](Proxy child){auto& c=m_nodes[child];return Area(Union(c.bounds,bounds))-(c.height?Area(c.bounds):0)+inheritance;};auto l=cost(n.left),r=cost(n.right);if(2*combined<l&&2*combined<r)break;sibling=l<r?n.left:n.right;}
 auto old=m_nodes[sibling].parent;auto parent=Allocate();m_nodes[parent]={Union(bounds,m_nodes[sibling].bounds),old,sibling,leaf,m_nodes[sibling].height+1,0};m_nodes[sibling].parent=parent;m_nodes[leaf].parent=parent;if(old==-1)m_root=parent;else if(m_nodes[old].left==sibling)m_nodes[old].left=parent;else m_nodes[old].right=parent;
 for(auto i=parent;i!=-1;i=m_nodes[i].parent){i=Balance(i);Fix(i);}
}
void SpatialIndex::Detach(Proxy leaf){if(m_root==leaf){m_root=-1;m_nodes[leaf].parent=-1;return;}auto parent=m_nodes[leaf].parent,grand=m_nodes[parent].parent;auto sibling=m_nodes[parent].left==leaf?m_nodes[parent].right:m_nodes[parent].left;if(grand==-1){m_root=sibling;m_nodes[sibling].parent=-1;}else{if(m_nodes[grand].left==parent)m_nodes[grand].left=sibling;else m_nodes[grand].right=sibling;m_nodes[sibling].parent=grand;for(auto i=grand;i!=-1;i=m_nodes[i].parent){i=Balance(i);Fix(i);}}Free(parent);m_nodes[leaf].parent=-1;}
#define TREE_QUERY(TEST) output.clear();m_stack.clear();if(m_root!=-1)m_stack.push_back(m_root);while(!m_stack.empty()){auto i=m_stack.back();m_stack.pop_back();auto& n=m_nodes[i];if(!(TEST))continue;if(n.left==-1)output.push_back(n.payload);else{m_stack.push_back(n.left);m_stack.push_back(n.right);}}
void SpatialIndex::Query(Aabb bounds,std::vector<std::uint32_t>& output)const{ENGINE_PROFILE_SCOPE("Scene.SpatialBoxQuery");TREE_QUERY(Overlaps(n.bounds,bounds))}
void SpatialIndex::Query(SpatialRay ray,std::vector<std::uint32_t>& output)const{ENGINE_PROFILE_SCOPE("Scene.SpatialRayQuery");TREE_QUERY(Intersects(n.bounds,ray))}
void SpatialIndex::Query(std::span<const Plane> planes,std::vector<std::uint32_t>& output)const{ENGINE_PROFILE_SCOPE("Scene.SpatialCull");TREE_QUERY(Intersects(n.bounds,planes))}
#undef TREE_QUERY
}

#![allow(dead_code)]

use std::marker::PhantomData;

mod gl;
mod shader;

pub struct Position {
    x: u32,
    y: u32,
}

pub struct Size {
    width: u32,
    height: u32,
}

pub struct Quad {
    pos: Position,
    size: Size,
}

pub trait SizingAlgorithm {
    type State;
    fn size(self, state: Self::State) -> Size;
}

pub struct FixedSizing(Size);
impl SizingAlgorithm for FixedSizing {
    type State = ();
    fn size(self, _: ()) -> Size {
        self.0
    }
}

// Layout lays out children. ShrinkSizing just means the size is whatever space
// laying out the children took up
pub struct ShrinkSizing<Layout: LayoutAlgorithm>(PhantomData<Layout>);
impl<Layout: LayoutAlgorithm> SizingAlgorithm for ShrinkSizing<Layout> {
    type State = Layout;
    fn size(self, layout: Layout) -> Size {
        layout.head()
    }
}

pub trait LayoutAlgorithm {
    fn layout_child(&mut self, parent_pos: &Position, child_size: &Size) -> Position;
    fn head(self) -> Size;
}

// currently we cant say which direction to start laying out from (ltr vs rtl, etc) as there is an
// assumption that &pos is always the top left
pub struct LinearLayout<Axis_: Axis> {
    pub head: Size,
    pub _marker: PhantomData<Axis_>,
}

impl<Axis_: Axis> LinearLayout<Axis_> {
    fn new() -> Self {
        Self {
            head: Size {
                width: 0,
                height: 0,
            },
            _marker: PhantomData,
        }
    }
}

pub trait Axis {
    fn advance(head: &mut Size, child: &Size);
}
pub struct HorizontalAxis;
pub struct VerticalAxis;
impl Axis for HorizontalAxis {
    fn advance(head: &mut Size, child: &Size) {
        head.width += child.width;
        head.height = std::cmp::max(head.height, child.height);
    }
}
impl Axis for VerticalAxis {
    #[inline]
    fn advance(head: &mut Size, child: &Size) {
        head.width = std::cmp::max(head.width, child.width);
        head.height += child.height;
    }
}

impl<Axis_: Axis> LayoutAlgorithm for LinearLayout<Axis_> {
    fn layout_child(&mut self, parent_pos: &Position, child_size: &Size) -> Position {
        let pos = Position {
            x: parent_pos.x + self.head.width,
            y: parent_pos.y + self.head.height,
        };
        Axis_::advance(&mut self.head, child_size);
        pos
    }

    fn head(self) -> Size {
        self.head
    }
}

pub trait Resolvable<T> {}
pub struct Resolved<T>(T);
impl<T> Resolvable<T> for Resolved<T> {}
pub struct Unresolved<T>(PhantomData<T>);
impl<T> Resolvable<T> for Unresolved<T> {}

pub struct UIElement<
    Position_: Resolvable<Position>,
    Size_: Resolvable<Size>,
    SizingAlgorithm_: SizingAlgorithm,
    LayoutAlgorithm_: LayoutAlgorithm,
> {
    pub pos: Position_,
    pub size: Size_,
    pub sizing: SizingAlgorithm_,
    pub layout: LayoutAlgorithm_,
}

impl<Layout: LayoutAlgorithm>
    UIElement<Resolved<Position>, Unresolved<Size>, ShrinkSizing<Layout>, Layout>
{
    pub fn new(pos: Position, layout: Layout) -> Self {
        UIElement {
            pos: Resolved(pos),
            size: Unresolved(PhantomData),
            sizing: ShrinkSizing(PhantomData),
            layout,
        }
    }
    pub fn add_child(
        &mut self,
        child: UIElement<
            Unresolved<Position>,
            Resolved<Size>,
            impl SizingAlgorithm,
            impl LayoutAlgorithm,
        >,
    ) -> Quad {
        Quad {
            pos: self.layout.layout_child(&self.pos.0, &child.size.0),
            size: child.size.0,
        }
    }

    pub fn finalize(self) -> Quad {
        Quad {
            pos: self.pos.0,
            size: self.sizing.size(self.layout),
        }
    }
}

fn main() {
    UIElement::new(
        Position { x: 10, y: 20 },
        LinearLayout::<HorizontalAxis>::new(),
    )
    .finalize();
}

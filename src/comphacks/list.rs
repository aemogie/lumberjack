use core::{any::type_name_of_val, marker::PhantomData};

#[derive(Debug)]
struct ConsWithCtx<Car, Cdr, _IGNORED> {
    pub car: Car,
    pub cdr: Cdr,
    _ignore: PhantomData<_IGNORED>,
}
pub type Cons<Car, Cdr> = ConsWithCtx<Car, Cdr, ()>;
impl<Car, Cdr> Cons<Car, Cdr> {
    #[inline(always)]
    const fn new(car: Car, cdr: Cdr) -> Self {
        ConsWithCtx {
            car,
            cdr,
            _ignore: PhantomData,
        }
    }
}

#[derive(Debug)]
struct NilWithCtx<_IGNORED>(PhantomData<_IGNORED>);
pub type Nil = NilWithCtx<()>;
impl Nil {
    #[inline(always)]
    const fn new() -> Self {
        NilWithCtx(PhantomData)
    }
}

#[inline(always)] // alias
pub fn cons_list() -> Nil {
    Nil::new()
}

pub trait ConsList {
    fn push<Car>(self, car: Car) -> Cons<Car, Self>
    where
        Self: Sized,
    {
        Cons::new(car, self)
    }
}
impl<_I> ConsList for NilWithCtx<_I> {}
impl<Car, Cdr: ConsList, _I> ConsList for ConsWithCtx<Car, Cdr, _I> {}

impl<Car, Cdr: ConsList> Cons<Car, Cdr> {
    pub fn pop(self) -> (Car, Cdr) {
        (self.car, self.cdr)
    }
}

trait TypeMapper<Input: ?Sized> {
    type Output;
    fn map(input: Input) -> Self::Output;
}

trait Map: ConsList {
    type Mapper;
    type Mapped: ConsList;
    fn map2(self) -> Self::Mapped;
}

impl<Mapper> Map for NilWithCtx<Mapper> {
    type Mapper = Mapper;
    type Mapped = Nil;

    #[inline(always)]
    fn map2(self) -> Self::Mapped {
        Nil::new()
    }
}

impl<Car, CarCdr, CdrCdr, Mapper> Map for ConsWithCtx<Car, Cons<CarCdr, CdrCdr>, Mapper>
where
    CdrCdr: ConsList,
    Mapper: TypeMapper<Car>,
    Mapper: TypeMapper<CarCdr>,
{
    type Mapper = Mapper;
    type Mapped = Cons<<Mapper as TypeMapper<Car>>::Output, Cdr::Mapped>;

    #[inline(always)]
    fn map2(self) -> Self::Mapped {
        let Self { car, cdr, .. } = self;
        let next = ConsWithCtx {
            car: cdr.car,
            cdr: cdr.cdr,
            _ignore: PhantomData::<Mapper>,
        };
        let cdr = next.map2();
        let car = Mapper::map(car);
        Cons::new(car, cdr)
    }
}

impl<Car, Cdr: ConsList + Map> Cons<Car, Cdr> {
    fn make_map<Mapper: TypeMapper<Car> + TypeMapper<Cdr>>(self) {
        (ConsWithCtx {
            car: self.car,
            cdr: self.cdr,
            _ignore: PhantomData::<Mapper>,
        })
        .map2();
    }
}

struct BoxMapper;
impl<Ty> TypeMapper<Ty> for BoxMapper {
    type Output = Box<Ty>;

    fn map(input: Ty) -> Self::Output {
        Box::new(input)
    }
}

pub fn foo() {
    let foo = cons_list().push(1).push(2);
    println!("{:#?}", foo);
    let boxed = foo.make_map::<BoxMapper>();
    // // type TestOutput = <TestInput as Map>::Mapped<BoxMapper>;
    println!("{}", type_name_of_val(&boxed));
}

class Employee {
    private String name;
    private int age;
    private String department;

    public Employee(String name, int age, String department) {
        this.name = name;
        this.age = age;
        this.department = department;
    }

    // getrs setrs
		public String getName() { return name; }
		public void setName(String name) { this.name = name; } 
		public int getAge() { return age; }
		public void setAge(int age) { this.age = age; }
		public String getDepartment() { return department; }
		public void setDepartment(String department) { this.department = department; }
}

class Manager extends Employee {
    public Manager(String name, int age, String department) {
        super(name, age, department);
    }
}

class Developer extends Employee {
    public Developer(String name, int age, String department) {
        super(name, age, department);
    }
}

class Designer extends Employee {
    public Designer(String name, int age, String department) {
        super(name, age, department);
    }
}
